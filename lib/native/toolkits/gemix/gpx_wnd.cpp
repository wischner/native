//
// Implements the GEMix window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>

#include <gem.h>

#include <native.h>

#include "gpx_wnd.h"
#include "gpx_img.h"
#include "globals.h"

namespace
{
    WORD gem_color(native::rgba color) {
        const int brightness = static_cast<int>(color.r) +
                               static_cast<int>(color.g) +
                               static_cast<int>(color.b);
        return brightness > (255 * 3 / 2) ? WHITE : BLACK;
    }

    void clip_to_vdi(const native::rect &r) {
        WORD clip[4] = {static_cast<WORD>(r.p.x),
                        static_cast<WORD>(r.p.y),
                        static_cast<WORD>(r.p.x + r.d.w - 1),
                        static_cast<WORD>(r.p.y + r.d.h - 1)};
        vs_clip(linux::gemix::runtime.vdi_handle, 1, clip);
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
        clip_to_vdi(r);
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
        clip_to_vdi(r);
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
        r.p.x += _offset.x;
        r.p.y += _offset.y;
        clip_to_vdi(rect(_clip.p.x + _offset.x,
                         _clip.p.y + _offset.y,
                         _clip.d.w,
                         _clip.d.h));
        if (filled) {
            vsf_interior(linux::gemix::runtime.vdi_handle, FIS_SOLID);
            vsf_color(linux::gemix::runtime.vdi_handle,
                      gem_color(get_ink()));
            WORD pxy[4] = {static_cast<WORD>(r.p.x),
                           static_cast<WORD>(r.p.y),
                           static_cast<WORD>(r.p.x + r.d.w - 1),
                           static_cast<WORD>(r.p.y + r.d.h - 1)};
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
        clip_to_vdi(r);
        vst_color(linux::gemix::runtime.vdi_handle,
                  gem_color(get_ink()));
        v_gtext(linux::gemix::runtime.vdi_handle,
                static_cast<WORD>(p.x + _offset.x),
                static_cast<WORD>(p.y + _offset.y +
                                  get_font_metrics().ascent),
                reinterpret_cast<const BYTE *>(text.c_str()));
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        rect clip(_clip.p.x + _offset.x,
                  _clip.p.y + _offset.y,
                  _clip.d.w,
                  _clip.d.h);
        clip_to_vdi(clip);
        const bool has_alpha = std::any_of(
            src.pixels(),
            src.pixels() +
                static_cast<std::size_t>(src.w()) * src.h(),
            [](rgba pixel) { return pixel.a != 255; });
        if (has_alpha) {
            constexpr std::uint8_t bayer[16] = {
                0,  8,  2,  10,
                12, 4,  14, 6,
                3,  11, 1,  9,
                15, 7,  13, 5};
            vsl_width(linux::gemix::runtime.vdi_handle, 1);
            for (int y = 0; y < src.h(); ++y) {
                int x = 0;
                while (x < src.w()) {
                    const rgba pixel = src.pixels()[y * src.w() + x];
                    const unsigned threshold =
                        static_cast<unsigned>(bayer[(y & 3) * 4 +
                                                    (x & 3)]) *
                        16U;
                    if (pixel.a <= threshold) {
                        ++x;
                        continue;
                    }
                    const WORD color = gem_color(pixel);
                    const int start = x;
                    ++x;
                    while (x < src.w()) {
                        const rgba next =
                            src.pixels()[y * src.w() + x];
                        const unsigned next_threshold =
                            static_cast<unsigned>(
                                bayer[(y & 3) * 4 + (x & 3)]) *
                            16U;
                        if (next.a <= next_threshold ||
                            gem_color(next) != color)
                            break;
                        ++x;
                    }
                    vsl_color(linux::gemix::runtime.vdi_handle,
                              color);
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

        MFDB src_mfdb{};
        MFDB dst_mfdb{};
        src_mfdb.fd_addr = const_cast<rgba *>(src.pixels());
        src_mfdb.fd_w = src.w();
        src_mfdb.fd_h = src.h();
        src_mfdb.fd_wdwidth = static_cast<WORD>((src.w() + 15) / 16);
        src_mfdb.fd_stand = 0;
        src_mfdb.fd_nplanes = 32;

        WORD pxy[8] = {
            0,
            0,
            static_cast<WORD>(src.w() - 1),
            static_cast<WORD>(src.h() - 1),
            static_cast<WORD>(dst.x + _offset.x),
            static_cast<WORD>(dst.y + _offset.y),
            static_cast<WORD>(dst.x + _offset.x + src.w() - 1),
            static_cast<WORD>(dst.y + _offset.y + src.h() - 1)};
        vro_cpyfm(linux::gemix::runtime.vdi_handle,
                  S_ONLY,
                  pxy,
                  &src_mfdb,
                  &dst_mfdb);
        vs_clip(linux::gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }
} // namespace native
