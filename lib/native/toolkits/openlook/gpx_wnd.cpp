//
// Implements the XView window graphics context with an Xlib
// backbuffer shared by portable paint and OPEN LOOK theme rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>

#include "../../platforms/linux/x_image.h"
#include "globals.h"
#include "gpx_wnd.h"

namespace
{
    void apply_gc(Display *display,
                  linux::openlook::openlook_gpx *cache,
                  native::gpx_wnd *graphics) {
        if (!cache || !cache->gc)
            return;
        if (cache->current_ink != graphics->get_ink()) {
            XSetForeground(
                display,
                cache->gc,
                native::detail::x_pixel(
                    DefaultVisual(
                        display, DefaultScreen(display)),
                    graphics->get_ink()));
            cache->current_ink = graphics->get_ink();
        }
        if (cache->current_thickness != graphics->get_pen()) {
            XSetLineAttributes(display,
                               cache->gc,
                               graphics->get_pen(),
                               LineSolid,
                               CapButt,
                               JoinMiter);
            cache->current_thickness = graphics->get_pen();
        }
        const native::rect clip = graphics->get_clip();
        XRectangle native_clip = {
            static_cast<short>(clip.p.x),
            static_cast<short>(clip.p.y),
            static_cast<unsigned short>(clip.d.w),
            static_cast<unsigned short>(clip.d.h)};
        XSetClipRectangles(
            display, cache->gc, 0, 0, &native_clip, 1, Unsorted);
    }
} // namespace

namespace native
{
    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        Display *display = linux::openlook::cached_display;
        if (!display) {
            throw std::runtime_error(
                "OpenLook/XView: no display for gpx_wnd.");
        }

        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!cache) {
            const Window drawable = linux::openlook::drawable(_wnd);
            if (drawable == None) {
                throw std::runtime_error(
                    "OpenLook/XView: window is not drawable.");
            }
            XWindowAttributes attributes = {};
            if (!XGetWindowAttributes(
                    display, drawable, &attributes)) {
                throw std::runtime_error(
                    "OpenLook/XView: unable to inspect drawable.");
            }
            cache = new linux::openlook::openlook_gpx;
            cache->gc = XCreateGC(display, drawable, 0, nullptr);
            cache->backbuffer = XCreatePixmap(
                display,
                drawable,
                static_cast<unsigned int>(attributes.width),
                static_cast<unsigned int>(attributes.height),
                static_cast<unsigned int>(attributes.depth));
            cache->buffer_width = attributes.width;
            cache->buffer_height = attributes.height;
            XSetForeground(display,
                           cache->gc,
                           WhitePixel(
                               display, DefaultScreen(display)));
            XFillRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           0,
                           0,
                           attributes.width,
                           attributes.height);
            linux::openlook::wnd_gpx_bindings.register_pair(
                _wnd, cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!cache)
            return;
        Display *display = linux::openlook::cached_display;
        if (cache->gc && display)
            XFreeGC(display, cache->gc);
        if (cache->backbuffer && display)
            XFreePixmap(display, cache->backbuffer);
        linux::openlook::wnd_gpx_bindings.unregister_by_handle(_wnd);
        delete cache;
    }

    gpx &gpx_wnd::set_clip(const rect &bounds) {
        _clip = bounds;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        Display *display = linux::openlook::cached_display;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;
        XSetForeground(
            display,
            cache->gc,
            detail::x_pixel(
                DefaultVisual(display, DefaultScreen(display)),
                color));
        XRectangle clip = {
            static_cast<short>(_clip.p.x),
            static_cast<short>(_clip.p.y),
            static_cast<unsigned short>(_clip.d.w),
            static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(
            display, cache->gc, 0, 0, &clip, 1, Unsorted);
        cache->current_ink = color;
        XFillRectangle(display,
                       cache->backbuffer,
                       cache->gc,
                       _clip.p.x,
                       _clip.p.y,
                       _clip.d.w,
                       _clip.d.h);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        Display *display = linux::openlook::cached_display;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;
        apply_gc(display, cache, this);
        XDrawLine(display,
                  cache->backbuffer,
                  cache->gc,
                  from.x,
                  from.y,
                  to.x,
                  to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect bounds, bool filled) {
        Display *display = linux::openlook::cached_display;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;
        apply_gc(display, cache, this);
        if (filled) {
            XFillRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           bounds.p.x,
                           bounds.p.y,
                           bounds.d.w,
                           bounds.d.h);
        } else {
            XDrawRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           bounds.p.x,
                           bounds.p.y,
                           bounds.d.w - 1,
                           bounds.d.h - 1);
        }
        return *this;
    }

    gpx &gpx_wnd::draw_native_text(
        const std::string &text, point position) {
        if (_font && !_font->valid())
            return *this;
        Display *display = linux::openlook::cached_display;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;
        apply_gc(display, cache, this);
        auto *font = linux::openlook::font_bindings
                         .object_from_handle(get_font().id());
        if (font && font->xfont)
            XSetFont(display, cache->gc, font->xfont);
        const int baseline =
            position.y + get_font_metrics().ascent;
        XDrawString(display,
                    cache->backbuffer,
                    cache->gc,
                    position.x,
                    baseline,
                    text.c_str(),
                    static_cast<int>(text.size()));
        return *this;
    }

    gpx &gpx_wnd::draw_img(
        const img &source, point destination) {
        Display *display = linux::openlook::cached_display;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(_wnd);
        if (!display || !cache || !cache->backbuffer)
            return *this;
        apply_gc(display, cache, this);
        detail::blend_x_image(
            display,
            cache->backbuffer,
            cache->gc,
            source,
            destination,
            _clip,
            size(cache->buffer_width, cache->buffer_height));
        return *this;
    }
} // namespace native
