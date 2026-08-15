//
// Implements the X11 image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <algorithm>

#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"
#include "../../platforms/linux/x_image.h"
#include "../../software_image.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image), _clip(0, 0, image.w(), image.h()) {
    }

    gpx &gpx_img::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_img::get_clip() const {
        return _clip;
    }

    gpx &gpx_img::clear(rgba color) {
        detail::clear_image(_img, _clip, color);
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to) {
        detail::draw_image_line(
            _img, _clip, from, to, _ink, _thickness);
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled) {
        detail::draw_image_rect(
            _img, _clip, r, _ink, _thickness, filled);
        return *this;
    }

    gpx &gpx_img::draw_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        Display *display = linux::x11::cached_display;
        if (!display)
            return *this;
        const int screen = DefaultScreen(display);
        Visual *visual = DefaultVisual(display, screen);
        Pixmap pixmap = XCreatePixmap(
            display,
            DefaultRootWindow(display),
            _img.w(),
            _img.h(),
            DefaultDepth(display, screen));
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XImage *upload = detail::x_image_from_rgba(
            display, _img.pixels(), _img.w(), _img.h());
        XPutImage(
            display, pixmap, gc, upload,
            0, 0, 0, 0, _img.w(), _img.h());
        XDestroyImage(upload);
        XSetForeground(display, gc, detail::x_pixel(visual, _ink));
        auto *font = linux::x11::font_bindings.object_from_handle(
            get_font().id());
        if (font && font->xfont)
            XSetFont(display, gc, font->xfont);

        XRectangle xr = {
            static_cast<short>(_clip.p.x),
            static_cast<short>(_clip.p.y),
            static_cast<unsigned short>(_clip.d.w),
            static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(display, gc, 0, 0, &xr, 1, Unsorted);

        const int baseline = p.y + get_font_metrics().ascent;
        XDrawString(
            display,
            pixmap,
            gc,
            p.x,
            baseline,
            text.c_str(),
            text.length());

        XImage *result = XGetImage(
            display,
            pixmap,
            0, 0,
            _img.w(), _img.h(),
            AllPlanes,
            ZPixmap);
        if (result) {
            rgba *destination = const_cast<rgba *>(_img.pixels());
            for (int y = 0; y < _img.h(); ++y) {
                for (int x = 0; x < _img.w(); ++x) {
                    rgba converted = detail::rgba_from_x_pixel(
                        visual, XGetPixel(result, x, y));
                    rgba &old = destination[y * _img.w() + x];
                    const bool changed = converted.r != old.r ||
                        converted.g != old.g || converted.b != old.b;
                    converted.a = changed ? _ink.a : old.a;
                    old = converted;
                }
            }
            XDestroyImage(result);
        }
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
