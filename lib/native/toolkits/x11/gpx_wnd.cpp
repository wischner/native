//
// Implements the X11 window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"
#include "scale.h"
#include "../../platforms/linux/x_image.h"

namespace
{
    // Thin pens draw one device pixel at every scale; wider pens
    // cover the device pixels of their logical width.
    bool thick_pen(int pen) {
        return pen > 1 && linux::x11::coordinate_scale() != 1.0;
    }

    // Device position of the stroke through logical pixel coordinate c.
    int stroke_position(int c, int pen) {
        const int cell = linux::x11::to_device(1);
        return linux::x11::to_device(c) +
               (thick_pen(pen) ? cell / 2 : (cell - 1) / 2);
    }

    XRectangle device_clip(const native::rect &logical) {
        const native::rect clip = linux::x11::rect_to_device(logical);
        return {static_cast<short>(clip.p.x),
                static_cast<short>(clip.p.y),
                static_cast<unsigned short>(clip.d.w),
                static_cast<unsigned short>(clip.d.h)};
    }

    // Repeat each image pixel over the device pixels it covers.
    std::unique_ptr<native::img> device_image(const native::img &source) {
        const int width = std::max(1, linux::x11::to_device(source.w()));
        const int height = std::max(1, linux::x11::to_device(source.h()));
        auto result = std::make_unique<native::img>(width, height);
        native::rgba *pixels = result->pixels();
        for (int y = 0; y < height; ++y) {
            const int source_y = y * source.h() / height;
            for (int x = 0; x < width; ++x)
                pixels[y * width + x] = source.pixels()[
                    source_y * source.w() + x * source.w() / width];
        }
        return result;
    }
} // namespace

// Apply portable drawing state to the cached X11 graphics context.
static void apply_gc(Display *display,
                     linux::x11::x11_gpx *cache,
                     native::gpx_wnd *self) {
    if (!cache || !cache->gc)
        return;

    if (cache->current_fg != self->get_ink()) {
        XSetForeground(
            display,
            cache->gc,
            native::detail::x_pixel(
                DefaultVisual(display, DefaultScreen(display)),
                self->get_ink()));
        cache->current_fg = self->get_ink();
    }

    if (cache->current_thickness != self->get_pen()) {
        // A scaled wide pen covers whole device pixels, endpoints
        // included, as a thin line does.
        const int pen = self->get_pen();
        const bool thick = thick_pen(pen);
        XSetLineAttributes(display,
                           cache->gc,
                           thick ? linux::x11::to_device(pen) : pen,
                           LineSolid,
                           thick ? CapProjecting : CapButt,
                           JoinMiter);
        cache->current_thickness = self->get_pen();
    }

    XRectangle xclip = device_clip(self->get_clip());
    XSetClipRectangles(display, cache->gc, 0, 0, &xclip, 1, Unsorted);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        if (!linux::x11::cached_display)
            throw std::runtime_error(
                "X11: No display available for gpx_wnd");

        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            Display *display = linux::x11::cached_display;
            Widget widget =
                linux::x11::wnd_bindings.handle_from_object(_wnd);
            if (!widget)
                throw std::runtime_error(
                    "X11/Athena: Missing drawing widget.");
            // A created but not yet shown widget can measure text and
            // render offscreen. Do not realize/map its shell just to
            // obtain a drawable for a same-screen pixmap and GC.
            Window win = RootWindowOfScreen(XtScreen(widget));
            int screen = DefaultScreen(display);
            Dimension width = 1, height = 1;
            XtVaGetValues(widget, XtNwidth, &width, XtNheight, &height,
                          nullptr);

            cache = new linux::x11::x11_gpx();
            cache->gc = XCreateGC(display, win, 0, nullptr);
            cache->backbuffer =
                XCreatePixmap(display,
                              win,
                              width,
                              height,
                              DefaultDepth(display, screen));
            cache->buf_w = width;
            cache->buf_h = height;

            // Start with a white backbuffer.
            XSetForeground(
                display, cache->gc, WhitePixel(display, screen));
            XFillRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           0,
                           0,
                           width,
                           height);

            linux::x11::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        if (cache->gc && linux::x11::cached_display)
            XFreeGC(linux::x11::cached_display, cache->gc);
        if (cache->backbuffer && linux::x11::cached_display) {
            XFreePixmap(linux::x11::cached_display, cache->backbuffer);
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
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        XSetForeground(
            display,
            cache->gc,
            detail::x_pixel(
                DefaultVisual(display, DefaultScreen(display)), color));
        XRectangle clip = device_clip(_clip);
        XSetClipRectangles(
            display, cache->gc, 0, 0, &clip, 1, Unsorted);
        cache->current_fg = color; // keep cache in sync so apply_gc
                                   // re-sets ink on next draw
        XFillRectangle(display,
                       cache->backbuffer,
                       cache->gc,
                       clip.x,
                       clip.y,
                       clip.width,
                       clip.height);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        Display *display = linux::x11::cached_display;
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);
        const int pen = get_pen();
        XDrawLine(display,
                  cache->backbuffer,
                  cache->gc,
                  stroke_position(from.x, pen),
                  stroke_position(from.y, pen),
                  stroke_position(to.x, pen),
                  stroke_position(to.y, pen));
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        Display *display = linux::x11::cached_display;
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        if (filled) {
            const rect device = linux::x11::rect_to_device(r);
            XFillRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           device.p.x,
                           device.p.y,
                           device.d.w,
                           device.d.h);
        } else if (thick_pen(get_pen())) {
            // Stroke the centres of the scaled edge pixels.
            const int pen = get_pen();
            const int left = stroke_position(r.p.x, pen);
            const int top = stroke_position(r.p.y, pen);
            XDrawRectangle(
                display,
                cache->backbuffer,
                cache->gc,
                left,
                top,
                stroke_position(r.p.x + int(r.d.w) - 1, pen) - left,
                stroke_position(r.p.y + int(r.d.h) - 1, pen) - top);
        } else {
            // A thin outline traces the outer device edge.
            const rect device = linux::x11::rect_to_device(r);
            XDrawRectangle(display,
                           cache->backbuffer,
                           cache->gc,
                           device.p.x,
                           device.p.y,
                           device.d.w - 1,
                           device.d.h - 1);
        }
        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        Display *display = linux::x11::cached_display;
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        auto *fh = linux::x11::font_bindings.object_from_handle(
            get_font().id());
        if (fh && (fh->device_xfont || fh->xfont))
            XSetFont(display,
                     cache->gc,
                     fh->device_xfont ? fh->device_xfont : fh->xfont);

        const int baseline = fh && fh->device_metrics
            ? linux::x11::to_device(p.y) + fh->device_metrics->ascent
            : linux::x11::to_device(p.y + get_font_metrics().ascent);
        XDrawString(display,
                    cache->backbuffer,
                    cache->gc,
                    linux::x11::to_device(p.x),
                    baseline,
                    text.c_str(),
                    text.length());
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        Display *display = linux::x11::cached_display;
        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        apply_gc(display, cache, this);

        // Collection imagery follows Athena's two-color control style.
        // Threshold after scaling so interpolation cannot add gray fringes.
        // Application canvases and other image surfaces retain full color.
        std::unique_ptr<img> monochrome;
        if (dynamic_cast<icon_view *>(_wnd) ||
            dynamic_cast<table_view *>(_wnd) ||
            dynamic_cast<tree_view *>(_wnd) ||
            dynamic_cast<accordion *>(_wnd)) {
            monochrome = std::make_unique<img>(src.w(), src.h());
            rgba *pixels = monochrome->pixels();
            for (int index = 0; index < src.w() * src.h(); ++index) {
                const rgba color = src.pixels()[index];
                const int brightness = 299 * color.r +
                    587 * color.g + 114 * color.b;
                const std::uint8_t value = brightness >= 224000 ? 255 : 0;
                pixels[index] = rgba(value, value, value, color.a >= 128 ? 255 : 0);
            }
        }
        const img &logical = monochrome ? *monochrome : src;
        std::unique_ptr<img> device;
        if (linux::x11::coordinate_scale() != 1.0)
            device = device_image(logical);
        detail::blend_x_image(display,
                              cache->backbuffer,
                              cache->gc,
                              device ? *device : logical,
                              point(linux::x11::to_device(dst.x),
                                    linux::x11::to_device(dst.y)),
                              linux::x11::rect_to_device(_clip),
                              size(cache->buf_w, cache->buf_h));
        return *this;
    }

} // namespace native
