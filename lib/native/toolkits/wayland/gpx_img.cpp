//
// Implements the Wayland image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>

#include <native.h>

#include "../../gpx_img.h"
#include "../../software_image.h"
#include "../../software_text.h"
#include "globals.h"

namespace native
{
    gpx_img::gpx_img(const img &image)
        : _img(image)
        , _clip(0, 0, image.w(), image.h()) {}

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

    gpx &gpx_img::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        auto *surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, _img.w(), _img.h());
        auto *cr = cairo_create(surface);
        linux::wayland::text(cr, text, p, _font, _ink);
        cairo_surface_flush(surface);
        const auto *pixels = reinterpret_cast<const std::uint32_t *>(
            cairo_image_surface_get_data(surface));
        const int stride = cairo_image_surface_get_stride(surface) / 4;
        native::img layer(_img.w(), _img.h());
        for (int y = 0; y < _img.h(); ++y)
            for (int x = 0; x < _img.w(); ++x) {
                const auto value = pixels[y * stride + x];
                const unsigned alpha = value >> 24;
                layer.pixels()[y * _img.w() + x] =
                    alpha ? rgba(((value >> 16) & 255) * 255 / alpha,
                                 ((value >> 8) & 255) * 255 / alpha,
                                 (value & 255) * 255 / alpha,
                                 alpha)
                          : rgba(0, 0, 0, 0);
            }
        detail::copy_image(_img, _clip, layer, point());
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }
} // namespace native
