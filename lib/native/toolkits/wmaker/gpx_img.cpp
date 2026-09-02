//
// Implements image graphics with the shared rasterizer and WINGs stock
// font drawing through a temporary X drawable.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <WINGs/WINGs.h>

#include <algorithm>

#include <native/graphics.h>

#include "../../gpx_img.h"
#include "../../platforms/linux/x_image.h"
#include "../../software_image.h"
#include "globals.h"

namespace native
{
    gpx_img::gpx_img(const img &image)
        : _img(image)
        , _clip(0, 0, image.w(), image.h()) {}

    gpx &gpx_img::set_clip(const rect &bounds) {
        _clip = bounds;
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

    gpx &gpx_img::draw_rect(rect bounds, bool filled) {
        detail::draw_image_rect(
            _img, _clip, bounds, _ink, _thickness, filled);
        return *this;
    }

    gpx &gpx_img::draw_native_text(
        const std::string &text, point position) {
        if (_font && !_font->valid())
            return *this;
        auto *font = linux::wmaker::font_bindings
                         .object_from_handle(get_font().id());
        if (!linux::wmaker::display || !font || !font->font)
            return *this;
        const int screen = DefaultScreen(linux::wmaker::display);
        Visual *visual = DefaultVisual(linux::wmaker::display, screen);
        Pixmap pixmap = XCreatePixmap(linux::wmaker::display,
                                      DefaultRootWindow(
                                          linux::wmaker::display),
                                      _img.w(),
                                      _img.h(),
                                      DefaultDepth(
                                          linux::wmaker::display,
                                          screen));
        GC gc = XCreateGC(
            linux::wmaker::display, pixmap, 0, nullptr);
        XImage *upload = detail::x_image_from_rgba(
            linux::wmaker::display,
            _img.pixels(),
            _img.w(),
            _img.h());
        XPutImage(linux::wmaker::display,
                  pixmap,
                  gc,
                  upload,
                  0,
                  0,
                  0,
                  0,
                  _img.w(),
                  _img.h());
        XDestroyImage(upload);
        WMColor *color = WMCreateRGBAColor(
            linux::wmaker::screen,
            static_cast<unsigned short>(_ink.r * 257U),
            static_cast<unsigned short>(_ink.g * 257U),
            static_cast<unsigned short>(_ink.b * 257U),
            static_cast<unsigned short>(_ink.a * 257U),
            False);
        if (color) {
            WMDrawString(linux::wmaker::screen,
                         pixmap,
                         color,
                         font->font,
                         position.x,
                         position.y,
                         text.c_str(),
                         static_cast<int>(text.size()));
            WMReleaseColor(color);
        }

        XImage *result = XGetImage(linux::wmaker::display,
                                   pixmap,
                                   0,
                                   0,
                                   _img.w(),
                                   _img.h(),
                                   AllPlanes,
                                   ZPixmap);
        if (result) {
            rgba *destination =
                const_cast<rgba *>(_img.pixels());
            for (int y = 0; y < _img.h(); ++y) {
                for (int x = 0; x < _img.w(); ++x) {
                    rgba converted = detail::rgba_from_x_pixel(
                        visual, XGetPixel(result, x, y));
                    rgba &old = destination[y * _img.w() + x];
                    const bool changed = converted.r != old.r ||
                                         converted.g != old.g ||
                                         converted.b != old.b;
                    if (!changed)
                        continue;

                    if (old.a == 0) {
                        // X/WINGs draws into an opaque pixmap. Recover the
                        // glyph coverage from the color interpolation rather
                        // than making every antialiased fringe pixel opaque;
                        // rotated native text otherwise gains a bright halo
                        // and becomes illegible on compact dark surfaces.
                        const auto coverage = [](std::uint8_t paper,
                                                 std::uint8_t ink,
                                                 std::uint8_t result) {
                            const int span = static_cast<int>(ink) - paper;
                            if (span == 0)
                                return 0;
                            const int delta =
                                static_cast<int>(result) - paper;
                            return std::clamp(
                                (delta * 255 + span / 2) / span,
                                0,
                                255);
                        };
                        int alpha = 0;
                        alpha = std::max(
                            alpha, coverage(old.r, _ink.r, converted.r));
                        alpha = std::max(
                            alpha, coverage(old.g, _ink.g, converted.g));
                        alpha = std::max(
                            alpha, coverage(old.b, _ink.b, converted.b));
                        old = rgba(
                            _ink.r,
                            _ink.g,
                            _ink.b,
                            static_cast<std::uint8_t>(
                                (alpha * _ink.a + 127) / 255));
                    } else {
                        converted.a = old.a;
                        old = converted;
                    }
                }
            }
            XDestroyImage(result);
        }
        XFreeGC(linux::wmaker::display, gc);
        XFreePixmap(linux::wmaker::display, pixmap);
        return *this;
    }

    gpx &gpx_img::draw_img(
        const img &source, point destination) {
        detail::copy_image(_img, _clip, source, destination);
        return *this;
    }
} // namespace native
