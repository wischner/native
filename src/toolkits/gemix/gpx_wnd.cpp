#include <algorithm>
#include <cstring>

#include <gem.h>

#include <native.h>

#include "gpx_wnd.h"
#include "gpx_img.h"
#include "globals.h"

namespace
{
    WORD gem_color(native::rgba color)
    {
        const int brightness = static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b);
        return brightness > (255 * 3 / 2) ? WHITE : BLACK;
    }

    void clip_to_vdi(const native::rect &r)
    {
        WORD clip[4] = {
            static_cast<WORD>(r.p.x),
            static_cast<WORD>(r.p.y),
            static_cast<WORD>(r.p.x + r.d.w - 1),
            static_cast<WORD>(r.p.y + r.d.h - 1)};
        vs_clip(gemix::runtime.vdi_handle, 1, clip);
    }
}

namespace native
{
    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        _clip = rect(0, 0, window->dimensions().w, window->dimensions().h);
    }

    gpx_wnd::~gpx_wnd() = default;

    gpx &gpx_wnd::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::clip() const
    {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color)
    {
        rect r = _clip;
        r.p.x += _offset.x;
        r.p.y += _offset.y;
        clip_to_vdi(r);
        vsf_interior(gemix::runtime.vdi_handle, FIS_SOLID);
        vsf_color(gemix::runtime.vdi_handle, gem_color(color));
        WORD pxy[4] = {
            static_cast<WORD>(r.p.x),
            static_cast<WORD>(r.p.y),
            static_cast<WORD>(r.p.x + r.d.w - 1),
            static_cast<WORD>(r.p.y + r.d.h - 1)};
        vr_recfl(gemix::runtime.vdi_handle, pxy);
        vs_clip(gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        rect r(_clip.p.x + _offset.x, _clip.p.y + _offset.y, _clip.d.w, _clip.d.h);
        clip_to_vdi(r);
        vsl_color(gemix::runtime.vdi_handle, gem_color(ink()));
        vsl_width(gemix::runtime.vdi_handle, pen());
        WORD pxy[4] = {
            static_cast<WORD>(from.x + _offset.x),
            static_cast<WORD>(from.y + _offset.y),
            static_cast<WORD>(to.x + _offset.x),
            static_cast<WORD>(to.y + _offset.y)};
        v_pline(gemix::runtime.vdi_handle, 2, pxy);
        vs_clip(gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        r.p.x += _offset.x;
        r.p.y += _offset.y;
        clip_to_vdi(rect(_clip.p.x + _offset.x, _clip.p.y + _offset.y, _clip.d.w, _clip.d.h));
        if (filled)
        {
            vsf_interior(gemix::runtime.vdi_handle, FIS_SOLID);
            vsf_color(gemix::runtime.vdi_handle, gem_color(ink()));
            WORD pxy[4] = {
                static_cast<WORD>(r.p.x),
                static_cast<WORD>(r.p.y),
                static_cast<WORD>(r.p.x + r.d.w - 1),
                static_cast<WORD>(r.p.y + r.d.h - 1)};
            vr_recfl(gemix::runtime.vdi_handle, pxy);
        }
        else
        {
            draw_line(point(r.p.x, r.p.y), point(r.p.x + r.d.w - 1, r.p.y));
            draw_line(point(r.p.x, r.p.y), point(r.p.x, r.p.y + r.d.h - 1));
            draw_line(point(r.p.x + r.d.w - 1, r.p.y), point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
            draw_line(point(r.p.x, r.p.y + r.d.h - 1), point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
        }
        vs_clip(gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        rect r(_clip.p.x + _offset.x, _clip.p.y + _offset.y, _clip.d.w, _clip.d.h);
        clip_to_vdi(r);
        vst_color(gemix::runtime.vdi_handle, gem_color(ink()));
        v_gtext(gemix::runtime.vdi_handle,
                static_cast<WORD>(p.x + _offset.x),
                static_cast<WORD>(p.y + _offset.y),
                reinterpret_cast<const BYTE *>(text.c_str()));
        vs_clip(gemix::runtime.vdi_handle, 0, nullptr);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        MFDB src_mfdb{};
        MFDB dst_mfdb{};
        src_mfdb.fd_addr = const_cast<rgba *>(src.pixels());
        src_mfdb.fd_w = src.w();
        src_mfdb.fd_h = src.h();
        src_mfdb.fd_wdwidth = static_cast<WORD>((src.w() + 15) / 16);
        src_mfdb.fd_stand = 0;
        src_mfdb.fd_nplanes = 32;

        WORD pxy[8] = {
            0, 0,
            static_cast<WORD>(src.w() - 1),
            static_cast<WORD>(src.h() - 1),
            static_cast<WORD>(dst.x + _offset.x),
            static_cast<WORD>(dst.y + _offset.y),
            static_cast<WORD>(dst.x + _offset.x + src.w() - 1),
            static_cast<WORD>(dst.y + _offset.y + src.h() - 1)};
        vro_cpyfm(gemix::runtime.vdi_handle, S_ONLY, pxy, &src_mfdb, &dst_mfdb);
        return *this;
    }
}
