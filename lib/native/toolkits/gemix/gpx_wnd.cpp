//
// Implements the GEMix window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <gem.h>

#include <cstdlib>
#include <cstring>

#include <native.h>

#include "gpx_wnd.h"
#include "gpx_img.h"
#include "globals.h"
#include "stock_text.h"

namespace
{
    bool inverse_raster_output() {
        const char *value = std::getenv("GEM_RASTA_INVERSE");
        if (!value || value[0] == '\0')
            value = std::getenv("RASTA_INVERSE");

        if (!value || value[0] == '\0')
            return true;

        return std::strcmp(value, "off") != 0 &&
               std::strcmp(value, "false") != 0 &&
               std::strcmp(value, "0") != 0;
    }

    WORD gem_color(native::rgba color) {
        const int brightness = static_cast<int>(color.r) +
                               static_cast<int>(color.g) +
                               static_cast<int>(color.b);
        const bool bright = brightness > (255 * 3 / 2);
        if (inverse_raster_output())
            return bright ? BLACK : WHITE;
        return bright ? WHITE : BLACK;
    }

    bool clip_to_vdi(native::rect r) {
        if (linux::gemix::runtime.painting)
            r = r.intersect(linux::gemix::runtime.paint_clip);
        if (r.w() == 0 || r.h() == 0) return false;
        WORD clip[4] = {static_cast<WORD>(r.p.x),
                        static_cast<WORD>(r.p.y),
                        static_cast<WORD>(r.p.x + r.d.w - 1),
                        static_cast<WORD>(r.p.y + r.d.h - 1)};
        vs_clip(linux::gemix::runtime.vdi_handle, 1, clip);
        vswr_mode(linux::gemix::runtime.vdi_handle, MD_REPLACE);
        return true;
    }
} // namespace

namespace native
{
    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        _clip = rect(0,
                     0,
                     window->get_dimensions().w,
                     window->get_dimensions().h);
    }

    gpx_wnd::~gpx_wnd() = default;

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        rect r = _clip;
        r.p.x += _offset.x;
        r.p.y += _offset.y;
        if (!clip_to_vdi(r)) return *this;
        vsf_interior(linux::gemix::runtime.vdi_handle, FIS_SOLID);
        vsf_color(linux::gemix::runtime.vdi_handle, gem_color(color));
        WORD pxy[4] = {static_cast<WORD>(r.p.x),
                       static_cast<WORD>(r.p.y),
                       static_cast<WORD>(r.p.x + r.d.w - 1),
                       static_cast<WORD>(r.p.y + r.d.h - 1)};
        vr_recfl(linux::gemix::runtime.vdi_handle, pxy);
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        rect r(_clip.p.x + _offset.x,
               _clip.p.y + _offset.y,
               _clip.d.w,
               _clip.d.h);
        if (!clip_to_vdi(r)) return *this;
        vsl_color(linux::gemix::runtime.vdi_handle,
                  gem_color(get_ink()));
        vsl_width(linux::gemix::runtime.vdi_handle, get_pen());
        WORD pxy[4] = {static_cast<WORD>(from.x + _offset.x),
                       static_cast<WORD>(from.y + _offset.y),
                       static_cast<WORD>(to.x + _offset.x),
                       static_cast<WORD>(to.y + _offset.y)};
        v_pline(linux::gemix::runtime.vdi_handle, 2, pxy);
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        if (r.w() == 0 || r.h() == 0) return *this;
        if (!clip_to_vdi(rect(_clip.p.x + _offset.x,
                         _clip.p.y + _offset.y,
                         _clip.d.w,
                         _clip.d.h))) return *this;
        if (filled) {
            const coord x = r.p.x + _offset.x;
            const coord y = r.p.y + _offset.y;
            vsf_interior(linux::gemix::runtime.vdi_handle, FIS_SOLID);
            vsf_color(linux::gemix::runtime.vdi_handle,
                      gem_color(get_ink()));
            WORD pxy[4] = {static_cast<WORD>(x),
                           static_cast<WORD>(y),
                           static_cast<WORD>(x + r.d.w - 1),
                           static_cast<WORD>(y + r.d.h - 1)};
            vr_recfl(linux::gemix::runtime.vdi_handle, pxy);
        } else {
            draw_line(point(r.p.x, r.p.y),
                      point(r.p.x + r.d.w - 1, r.p.y));
            draw_line(point(r.p.x, r.p.y),
                      point(r.p.x, r.p.y + r.d.h - 1));
            draw_line(point(r.p.x + r.d.w - 1, r.p.y),
                      point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
            draw_line(point(r.p.x, r.p.y + r.d.h - 1),
                      point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
        }
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        rect r(_clip.p.x + _offset.x,
               _clip.p.y + _offset.y,
               _clip.d.w,
               _clip.d.h);
        if (!clip_to_vdi(r)) return *this;
        vst_color(linux::gemix::runtime.vdi_handle,
                  gem_color(get_ink()));
        const auto encoded = linux::gemix::stock_text(text);
        v_gtext(linux::gemix::runtime.vdi_handle,
                static_cast<WORD>(p.x + _offset.x),
                static_cast<WORD>(p.y + _offset.y +
                                  get_font_metrics().ascent),
                reinterpret_cast<const BYTE *>(encoded.c_str()));
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        rect clip(_clip.p.x + _offset.x,
                  _clip.p.y + _offset.y,
                  _clip.d.w,
                  _clip.d.h);
        if (linux::gemix::runtime.painting)
            clip = clip.intersect(linux::gemix::runtime.paint_clip);
        const int origin_x = dst.x + _offset.x;
        const int origin_y = dst.y + _offset.y;
        clip = clip.intersect(rect(origin_x, origin_y, src.w(), src.h()));
        if (!clip_to_vdi(clip)) return *this;
        const int first_x = clip.x1() - origin_x;
        const int last_x = clip.x2() - origin_x;
        const int first_y = clip.y1() - origin_y;
        const int last_y = clip.y2() - origin_y;
        constexpr std::uint8_t bayer[16] = {
            0,  8,  2,  10,
            12, 4,  14, 6,
            3,  11, 1,  9,
            15, 7,  13, 5};
        bool colored = false;
        for (int index = 0; index < src.w() * src.h(); ++index) {
            const rgba pixel = src.pixels()[index];
            if (pixel.a != 0 &&
                (pixel.r != pixel.g || pixel.g != pixel.b)) {
                colored = true;
                break;
            }
        }

        const auto display_color = [colored, &bayer](
                                       rgba pixel,
                                       int x,
                                       int y) -> WORD {
            if (!colored)
                return gem_color(pixel);

            const unsigned luminance =
                (77U * pixel.r + 150U * pixel.g + 29U * pixel.b) >> 8;
            const unsigned threshold =
                static_cast<unsigned>(
                    bayer[(y & 3) * 4 + (x & 3)]) *
                    16U +
                8U;
            return gem_color(luminance >= threshold
                                 ? native::rgba(255, 255, 255, 255)
                                 : native::rgba(0, 0, 0, 255));
        };
        vsl_width(linux::gemix::runtime.vdi_handle, 1);
        for (int y = first_y; y < last_y; ++y) {
            int x = first_x;
            while (x < last_x) {
                const rgba pixel = src.pixels()[y * src.w() + x];
                const unsigned threshold =
                    static_cast<unsigned>(bayer[(y & 3) * 4 +
                                                (x & 3)]) *
                    16U;
                if (pixel.a <= threshold) {
                    ++x;
                    continue;
                }
                const WORD color = display_color(pixel, x, y);
                const int start = x;
                ++x;
                while (x < last_x) {
                    const rgba next =
                        src.pixels()[y * src.w() + x];
                    const unsigned next_threshold =
                        static_cast<unsigned>(
                            bayer[(y & 3) * 4 + (x & 3)]) *
                        16U;
                    if (next.a <= next_threshold ||
                        display_color(next, x, y) != color)
                        break;
                    ++x;
                }
                vsl_color(linux::gemix::runtime.vdi_handle, color);
                WORD line[4] = {
                    static_cast<WORD>(dst.x + _offset.x + start),
                    static_cast<WORD>(dst.y + _offset.y + y),
                    static_cast<WORD>(dst.x + _offset.x + x - 1),
                    static_cast<WORD>(dst.y + _offset.y + y)};
                v_pline(linux::gemix::runtime.vdi_handle, 2, line);
            }
        }
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }
} // namespace native
