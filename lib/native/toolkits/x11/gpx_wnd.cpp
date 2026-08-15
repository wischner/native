//
// Implements the X11 window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

// Apply ink and pen to the cached GC only when they have changed.
// No GC-level clip is set — the full backbuffer is always repainted,
// so clipping at the GC level would only cause artifacts on XCopyArea.
static void apply_gc(Display *display, linux::x11::x11_gpx *cache, native::gpx_wnd *self) {
    if (!cache || !cache->gc) return;

    if (cache->current_fg != self->get_ink()) {
        XSetForeground(display, cache->gc, self->get_ink());
        cache->current_fg = self->get_ink();
    }

    if (cache->current_thickness != self->get_pen()) {
        XSetLineAttributes(display, cache->gc, self->get_pen(), LineSolid, CapButt, JoinMiter);
        cache->current_thickness = self->get_pen();
    }
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset) {
        if (!linux::x11::cached_display)
            throw std::runtime_error("X11: No display available for gpx_wnd");

        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            Display *display = linux::x11::cached_display;
            Window win = linux::x11::wnd_bindings.handle_from_object(_wnd);
            int screen = DefaultScreen(display);

            // Get the actual current window size.
            XWindowAttributes attrs;
            XGetWindowAttributes(display, win, &attrs);

            cache = new linux::x11::x11_gpx();
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

            linux::x11::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        if (cache->gc && linux::x11::cached_display)
            XFreeGC(linux::x11::cached_display, cache->gc);
        if (cache->backbuffer && linux::x11::cached_display) {
            XFreePixmap(
                linux::x11::cached_display,
                cache->backbuffer);
        }
        delete cache;
        linux::x11::wnd_gpx_bindings.unregister_by_handle(_wnd);
    }

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        Display *display = linux::x11::cached_display;
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        XSetForeground(display, cache->gc, color);
        cache->current_fg = color; // keep cache in sync so apply_gc re-sets ink on next draw
        XFillRectangle(display, cache->backbuffer, cache->gc,
                       _clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        Display *display = linux::x11::cached_display;
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        apply_gc(display, cache, this);
        XDrawLine(display, cache->backbuffer, cache->gc,
                  from.x, from.y, to.x, to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        Display *display = linux::x11::cached_display;
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
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

    gpx &gpx_wnd::draw_text(const std::string &text, point p) {
        Display *display = linux::x11::cached_display;
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer) return *this;

        apply_gc(display, cache, this);

        auto *fh = linux::x11::font_bindings.object_from_handle(get_font().id());
        if (fh && fh->xfont)
            XSetFont(display, cache->gc, fh->xfont);

        XDrawString(display, cache->backbuffer, cache->gc,
                    p.x, p.y, text.c_str(), text.length());
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        Display *display = linux::x11::cached_display;
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
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
