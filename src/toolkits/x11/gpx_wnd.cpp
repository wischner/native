#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

// Apply ink and pen to the cached GC only when they have changed.
// No GC-level clip is set — the full backbuffer is always repainted,
// so clipping at the GC level would only cause artifacts on XCopyArea.
static void apply_gc(Display *display, x11::x11gpx *cache, native::gpx_wnd *self)
{
    if (!cache || !cache->gc) return;

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

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        if (!x11::cached_display)
            throw std::runtime_error("X11: No display available for gpx_wnd");

        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            Display *display = x11::cached_display;
            Window win = x11::wnd_bindings.from_b(_wnd);
            int screen = DefaultScreen(display);

            // Get the actual current window size.
            XWindowAttributes attrs;
            XGetWindowAttributes(display, win, &attrs);

            cache = new x11::x11gpx();
            cache->gc = XCreateGC(display, win, 0, nullptr);
            cache->backbuffer = XCreatePixmap(display, win,
                                              attrs.width, attrs.height,
                                              DefaultDepth(display, screen));
            cache->buf_w = attrs.width;
            cache->buf_h = attrs.height;

            // Start with a white backbuffer.
            XSetForeground(display, cache->gc, WhitePixel(display, screen));
            XFillRectangle(display, cache->backbuffer, cache->gc,
                           0, 0, attrs.width, attrs.height);

            x11::wnd_gpx_bindings.register_pair(_wnd, cache);
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
        Display *display = x11::cached_display;
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        XSetForeground(display, cache->gc, color);
        cache->current_fg = color; // keep cache in sync so apply_gc re-sets ink on next draw
        XFillRectangle(display, cache->backbuffer, cache->gc,
                       _clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        Display *display = x11::cached_display;
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        apply_gc(display, cache, this);
        XDrawLine(display, cache->backbuffer, cache->gc,
                  from.x, from.y, to.x, to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        Display *display = x11::cached_display;
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer) return *this;

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
        Display *display = x11::cached_display;
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer) return *this;

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
        Display *display = x11::cached_display;
        auto *cache = x11::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        apply_gc(display, cache, this);

        XImage *ximg = XCreateImage(display,
                                    DefaultVisual(display, DefaultScreen(display)),
                                    DefaultDepth(display, DefaultScreen(display)),
                                    ZPixmap, 0,
                                    reinterpret_cast<char *>(const_cast<rgba *>(src.pixels())),
                                    src.w(), src.h(), 32, 0);

        XPutImage(display, cache->backbuffer, cache->gc,
                  ximg, 0, 0, dst.x, dst.y, src.w(), src.h());
        XDestroyImage(ximg);
        return *this;
    }

} // namespace native
