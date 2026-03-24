#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <xview/xview.h>
#ifdef coord
#undef coord
#endif

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    Display *display_for_wnd(const native::wnd *owner)
    {
        Xv_opaque paint_window = openlook::canvas_bindings.from_b(const_cast<native::wnd *>(owner));
        Display *display = paint_window ? reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY)) : nullptr;
        if (!display)
            display = openlook::cached_display;
        if (display)
            openlook::cached_display = display;
        return display;
    }

    void apply_gc(Display *display, openlook::openlookgpx *cache, native::gpx_wnd *self)
    {
        if (!cache || !cache->gc)
            return;

        if (cache->current_fg != self->ink())
        {
            XSetForeground(display, cache->gc, self->ink());
            cache->current_fg = self->ink();
        }

        if (cache->current_thickness != self->pen())
        {
            XSetLineAttributes(display, cache->gc, self->pen(), LineSolid, CapButt, JoinMiter);
            cache->current_thickness = self->pen();
        }
    }
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        Xv_opaque paint_window = openlook::canvas_bindings.from_b(_wnd);
        if (!paint_window)
            throw std::runtime_error("OpenLook: No paint window available for gpx_wnd");

        Display *display = reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY));
        if (!display)
            display = openlook::cached_display;
        if (!display)
            throw std::runtime_error("OpenLook: No display available for gpx_wnd");
        openlook::cached_display = display;

        const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
        if (!xwin)
            throw std::runtime_error("OpenLook: No X11 window available for gpx_wnd");

        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            int screen = DefaultScreen(display);

            XWindowAttributes attrs;
            XGetWindowAttributes(display, xwin, &attrs);

            cache = new openlook::openlookgpx();
            cache->gc = XCreateGC(display, xwin, 0, nullptr);
            cache->backbuffer = XCreatePixmap(
                display,
                xwin,
                static_cast<unsigned int>(attrs.width),
                static_cast<unsigned int>(attrs.height),
                DefaultDepth(display, screen));
            cache->buf_w = attrs.width;
            cache->buf_h = attrs.height;

            XSetForeground(display, cache->gc, WhitePixel(display, screen));
            XFillRectangle(display, cache->backbuffer, cache->gc,
                           0, 0,
                           static_cast<unsigned int>(attrs.width),
                           static_cast<unsigned int>(attrs.height));

            openlook::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
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
        Display *display = display_for_wnd(_wnd);
        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;

        XSetForeground(display, cache->gc, color);
        cache->current_fg = color;
        XFillRectangle(display, cache->backbuffer, cache->gc,
                       _clip.p.x, _clip.p.y,
                       _clip.d.w, _clip.d.h);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        Display *display = display_for_wnd(_wnd);
        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);
        XDrawLine(display, cache->backbuffer, cache->gc,
                  from.x, from.y, to.x, to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        Display *display = display_for_wnd(_wnd);
        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        if (filled)
            XFillRectangle(display, cache->backbuffer, cache->gc,
                           r.p.x, r.p.y, r.d.w, r.d.h);
        else
            XDrawRectangle(display, cache->backbuffer, cache->gc,
                           r.p.x, r.p.y, r.d.w - 1, r.d.h - 1);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        Display *display = display_for_wnd(_wnd);
        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        Font font = XLoadFont(display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        XSetFont(display, cache->gc, font);
        XDrawString(display, cache->backbuffer, cache->gc,
                    p.x, p.y, text.c_str(), text.length());
        XUnloadFont(display, font);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        Display *display = display_for_wnd(_wnd);
        auto *cache = openlook::wnd_gpx_bindings.from_a(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        XImage *ximg = XCreateImage(
            display,
            DefaultVisual(display, DefaultScreen(display)),
            DefaultDepth(display, DefaultScreen(display)),
            ZPixmap,
            0,
            reinterpret_cast<char *>(const_cast<rgba *>(src.pixels())),
            src.w(), src.h(), 32, 0);
        if (!ximg)
            return *this;

        XPutImage(display, cache->backbuffer, cache->gc,
                  ximg, 0, 0,
                  dst.x, dst.y,
                  src.w(), src.h());
        ximg->data = nullptr;
        XDestroyImage(ximg);
        return *this;
    }

} // namespace native
