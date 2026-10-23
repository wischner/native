//
// Implements the GEMix window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <gem.h>
#include <vector>
#include <algorithm>

#include <native.h>

#include "gpx_wnd.h"
#include "gpx_img.h"
#include "globals.h"
#include "stock_text.h"
#include "../../property_grid_peer.h"

namespace
{
    WORD gem_color(native::rgba color) {
        const int brightness = static_cast<int>(color.r) +
                               static_cast<int>(color.g) +
                               static_cast<int>(color.b);
        const bool bright = brightness > (255 * 3 / 2);
        // The pinned hosted VDI maps index 0 to pixel 1 and other indices
        // to pixel 0. Its RGBA raster (also used directly by AES) displays
        // pixel 0 as white and pixel 1 as black. Match that physical palette;
        // viewer inversion is a session-wide presentation setting.
        return bright ? BLACK : WHITE;
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
        if (linux::gemix::runtime.painting)
            r = r.intersect(linux::gemix::runtime.paint_clip);
        const rect stroke(std::min(from.x, to.x) + _offset.x,
                          std::min(from.y, to.y) + _offset.y,
                          std::abs(int(to.x) - from.x) + get_pen(),
                          std::abs(int(to.y) - from.y) + get_pen());
        if (!r.intersect(stroke).w() || !r.intersect(stroke).h())
            return *this;
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
        if (r.w() == 0 || r.h() == 0 || !_clip.intersect(r).w() ||
            !_clip.intersect(r).h())
            return *this;
        if (linux::gemix::runtime.painting) {
            const rect absolute(
                r.p.x + _offset.x, r.p.y + _offset.y, r.w(), r.h());
            const auto visible =
                absolute.intersect(linux::gemix::runtime.paint_clip);
            if (!visible.w() || !visible.h())
                return *this;
        }
        if (!clip_to_vdi(rect(_clip.p.x + _offset.x,
                         _clip.p.y + _offset.y,
                         _clip.d.w,
                         _clip.d.h))) return *this;
        if (filled) {
            const coord x = r.p.x + _offset.x;
            const coord y = r.p.y + _offset.y;
            vsf_interior(linux::gemix::runtime.vdi_handle, FIS_SOLID);
            const bool grid_dot =
                r.w() == 1 && r.h() == 1 &&
                native::detail::property_grid_owner(*_wnd);
            vsf_color(
                linux::gemix::runtime.vdi_handle,
                gem_color(grid_dot ? rgba(0, 0, 0, 255) : get_ink()));
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
        if (linux::gemix::runtime.painting)
            r = r.intersect(linux::gemix::runtime.paint_clip);
        const auto metrics = get_font_metrics();
        const rect text_bounds(
            p.x + _offset.x,
            p.y + _offset.y,
            std::min<std::size_t>(
                65535, text.size() * std::max(1, metrics.max_advance)),
            metrics.height);
        const auto visible = r.intersect(text_bounds);
        if (!visible.w() || !visible.h())
            return *this;
        if (!clip_to_vdi(r)) return *this;
        vst_color(linux::gemix::runtime.vdi_handle,
                  gem_color(get_ink()));
        const auto encoded = linux::gemix::stock_text(text);
        // Hosted GEM's v_gtext origin is the bottom of the font cell.
        // Using the typographic ascent moves text above centered images.
        v_gtext(linux::gemix::runtime.vdi_handle,
                static_cast<WORD>(p.x + _offset.x),
                static_cast<WORD>(p.y + _offset.y +
                                  get_font_metrics().height),
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
        // Upload two monochrome masks instead of issuing a VDI call for
        // each horizontal pixel run. Transparent pixels belong to
        // neither.
        const int width = clip.w(), height = clip.h();
        const int stride = (width + 15) / 16;
        std::vector<UWORD> black(stride * height),
            white(stride * height);
        for (int y = first_y; y < last_y; ++y)
            for (int x = first_x; x < last_x; ++x) {
                const auto pixel = src.pixels()[y * src.w() + x];
                if (pixel.a <=
                    unsigned(bayer[(y & 3) * 4 + (x & 3)]) * 16U)
                    continue;
                auto &bits = display_color(pixel, x, y) ==
                                     gem_color(rgba(0, 0, 0, 255))
                                 ? black
                                 : white;
                bits[(y - first_y) * stride + (x - first_x) / 16] |=
                    UWORD(0x8000U >> ((x - first_x) % 16));
            }
        MFDB target{};
        WORD coordinates[8] = {0,
                               0,
                               WORD(width - 1),
                               WORD(height - 1),
                               WORD(clip.x1()),
                               WORD(clip.y1()),
                               WORD(clip.x2() - 1),
                               WORD(clip.y2() - 1)};
        for (int layer = 0; layer < 2; ++layer) {
            auto &bits = layer ? white : black;
            if (std::none_of(bits.begin(), bits.end(), [](UWORD value) {
                    return value != 0;
                }))
                continue;
            MFDB source{bits.data(),
                        WORD(width),
                        WORD(height),
                        WORD(stride),
                        1,
                        1,
                        0,
                        0,
                        0};
            const WORD color = gem_color(
                layer ? rgba(255, 255, 255, 255) : rgba(0, 0, 0, 255));
            WORD colors[2] = {gem_color(rgba(255, 255, 255, 255)),
                              color};
            vrt_cpyfm(linux::gemix::runtime.vdi_handle,
                      MD_TRANS,
                      coordinates,
                      &source,
                      &target,
                      colors);
        }
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }
} // namespace native
