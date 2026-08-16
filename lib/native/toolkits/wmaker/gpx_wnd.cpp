//
// Implements buffered Xlib drawing for Window Maker application
// windows while using WINGs for native UTF-8 font rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <WINGs/WINGs.h>

#include <native/graphics.h>

#include "../../gpx_wnd.h"
#include "../../platforms/linux/x_image.h"
#include "globals.h"

namespace
{
    void apply_gc(linux::wmaker::window_graphics *cache,
                  native::gpx_wnd *graphics) {
        if (!cache || !cache->gc)
            return;
        Display *display = linux::wmaker::display;
        if (cache->current_ink != graphics->get_ink()) {
            XSetForeground(
                display,
                cache->gc,
                native::detail::x_pixel(
                    DefaultVisual(display, DefaultScreen(display)),
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

    WMColor *make_color(native::rgba color) {
        return WMCreateRGBAColor(
            linux::wmaker::screen,
            static_cast<unsigned short>(color.r * 257U),
            static_cast<unsigned short>(color.g * 257U),
            static_cast<unsigned short>(color.b * 257U),
            static_cast<unsigned short>(color.a * 257U),
            False);
    }
} // namespace

namespace native
{
    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        if (!linux::wmaker::display) {
            throw std::runtime_error(
                "Window Maker/WINGs: no display for graphics.");
        }
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache) {
            const Window target = linux::wmaker::drawable(_wnd);
            if (target == None) {
                throw std::runtime_error(
                    "Window Maker/WINGs: window is not realized.");
            }
            XWindowAttributes attributes = {};
            if (!XGetWindowAttributes(
                    linux::wmaker::display, target, &attributes)) {
                throw std::runtime_error(
                    "Window Maker/WINGs: cannot inspect drawable.");
            }
            const size dimensions = window->get_dimensions();
            cache = new linux::wmaker::window_graphics;
            cache->gc = XCreateGC(
                linux::wmaker::display, target, 0, nullptr);
            cache->width = dimensions.w;
            cache->height = dimensions.h;
            cache->backbuffer = XCreatePixmap(
                linux::wmaker::display,
                target,
                static_cast<unsigned int>(cache->width),
                static_cast<unsigned int>(cache->height),
                static_cast<unsigned int>(attributes.depth));
            XSetForeground(
                linux::wmaker::display,
                cache->gc,
                WhitePixel(linux::wmaker::display,
                           DefaultScreen(linux::wmaker::display)));
            XFillRectangle(linux::wmaker::display,
                           cache->backbuffer,
                           cache->gc,
                           0,
                           0,
                           cache->width,
                           cache->height);
            linux::wmaker::graphics_bindings.register_pair(
                _wnd, cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache)
            return;
        if (cache->gc && linux::wmaker::display)
            XFreeGC(linux::wmaker::display, cache->gc);
        if (cache->backbuffer != None && linux::wmaker::display) {
            XFreePixmap(linux::wmaker::display, cache->backbuffer);
        }
        linux::wmaker::graphics_bindings.unregister_by_handle(_wnd);
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
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache || cache->backbuffer == None)
            return *this;
        XSetForeground(
            linux::wmaker::display,
            cache->gc,
            detail::x_pixel(
                DefaultVisual(linux::wmaker::display,
                              DefaultScreen(linux::wmaker::display)),
                color));
        XRectangle clip = {
            static_cast<short>(_clip.p.x),
            static_cast<short>(_clip.p.y),
            static_cast<unsigned short>(_clip.d.w),
            static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(linux::wmaker::display,
                           cache->gc,
                           0,
                           0,
                           &clip,
                           1,
                           Unsorted);
        cache->current_ink = color;
        XFillRectangle(linux::wmaker::display,
                       cache->backbuffer,
                       cache->gc,
                       _clip.p.x,
                       _clip.p.y,
                       _clip.d.w,
                       _clip.d.h);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache || cache->backbuffer == None)
            return *this;
        apply_gc(cache, this);
        XDrawLine(linux::wmaker::display,
                  cache->backbuffer,
                  cache->gc,
                  from.x,
                  from.y,
                  to.x,
                  to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect bounds, bool filled) {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache || cache->backbuffer == None)
            return *this;
        apply_gc(cache, this);
        if (filled) {
            XFillRectangle(linux::wmaker::display,
                           cache->backbuffer,
                           cache->gc,
                           bounds.p.x,
                           bounds.p.y,
                           bounds.d.w,
                           bounds.d.h);
        } else {
            XDrawRectangle(linux::wmaker::display,
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
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        auto *font = linux::wmaker::font_bindings
                         .object_from_handle(get_font().id());
        if (!cache || cache->backbuffer == None || !font ||
            !font->font) {
            return *this;
        }
        WMColor *color = make_color(_ink);
        if (!color)
            return *this;
        WMDrawString(linux::wmaker::screen,
                     cache->backbuffer,
                     color,
                     font->font,
                     position.x,
                     position.y,
                     text.c_str(),
                     static_cast<int>(text.size()));
        WMReleaseColor(color);
        return *this;
    }

    gpx &gpx_wnd::draw_img(
        const img &source, point destination) {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(_wnd);
        if (!cache || cache->backbuffer == None)
            return *this;
        apply_gc(cache, this);
        detail::blend_x_image(linux::wmaker::display,
                              cache->backbuffer,
                              cache->gc,
                              source,
                              destination,
                              _clip,
                              size(cache->width, cache->height));
        return *this;
    }
} // namespace native
