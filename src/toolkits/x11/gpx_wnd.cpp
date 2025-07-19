#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_gc(Display *display, GC gc, native::gpx_wnd *self, x11::x11gpx *cache)
{
    if (!cache) return;

    if (cache->current_fg != self->ink())
    {
        XSetForeground(display, gc, self->ink());
        cache->current_fg = self->ink();
    }

    if (cache->current_thickness != self->pen())
    {
        XSetLineAttributes(display, gc, self->pen(), LineSolid, CapButt, JoinMiter);
        cache->current_thickness = self->pen();
    }

    XRectangle xr = {
        static_cast<short>(self->clip().p.x),
        static_cast<short>(self->clip().p.y),
        static_cast<unsigned short>(self->clip().d.w),
        static_cast<unsigned short>(self->clip().d.h)};
    XSetClipRectangles(display, gc, 0, 0, &xr, 1, Unsorted);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        if (!x11::cached_display)
            throw std::runtime_error("X11: No display available for gpx_wnd");
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
        Display *display = x11::cached_display;
        Window win = x11::wnd_bindings.from_b(_wnd);

        GC gc = XCreateGC(display, win, 0, nullptr);
        XSetForeground(display, gc, color);

        XRectangle xr = {
            static_cast<short>(_clip.p.x),
            static_cast<short>(_clip.p.y),
            static_cast<unsigned short>(_clip.d.w),
            static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(display, gc, 0, 0, &xr, 1, Unsorted);

        XFillRectangle(display, win, gc, _clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        XFreeGC(display, gc);
        XFlush(display);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        Display *display = x11::cached_display;
        Window win = x11::wnd_bindings.from_b(_wnd);
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);

        GC gc = XCreateGC(display, win, 0, nullptr);
        apply_gc(display, gc, this, cache);

        XDrawLine(display, win, gc, from.x, from.y, to.x, to.y);

        XFreeGC(display, gc);
        XFlush(display);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        Display *display = x11::cached_display;
        Window win = x11::wnd_bindings.from_b(_wnd);
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);

        GC gc = XCreateGC(display, win, 0, nullptr);
        apply_gc(display, gc, this, cache);

        if (filled)
            XFillRectangle(display, win, gc, r.p.x, r.p.y, r.d.w, r.d.h);
        else
            XDrawRectangle(display, win, gc, r.p.x, r.p.y, r.d.w - 1, r.d.h - 1);

        XFreeGC(display, gc);
        XFlush(display);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        Display *display = x11::cached_display;
        Window win = x11::wnd_bindings.from_b(_wnd);
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);

        GC gc = XCreateGC(display, win, 0, nullptr);
        apply_gc(display, gc, this, cache);

        Font font = XLoadFont(display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        XSetFont(display, gc, font);

        XDrawString(display, win, gc, p.x, p.y, text.c_str(), text.length());

        XUnloadFont(display, font);
        XFreeGC(display, gc);
        XFlush(display);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        Display *display = x11::cached_display;
        Window win = x11::wnd_bindings.from_b(_wnd);
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);

        GC gc = XCreateGC(display, win, 0, nullptr);
        apply_gc(display, gc, this, cache);

        XImage *ximg = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)),
                                    DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
                                    reinterpret_cast<char *>(const_cast<rgba *>(src.pixels())),
                                    src.w(), src.h(), 32, 0);

        XPutImage(display, win, gc, ximg, 0, 0, dst.x, dst.y, src.w(), src.h());

        XDestroyImage(ximg);
        XFreeGC(display, gc);
        XFlush(display);
        return *this;
    }

} // namespace native
