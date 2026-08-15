//
// Implements the GEMix image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>

#include <native.h>

#include "gpx_img.h"
#include "../../software_image.h"
#include "../../software_text.h"
#include "globals.h"

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
        const int advance = linux::gemix::runtime.initialized
            ? linux::gemix::runtime.char_w
            : 8;
        const int height = linux::gemix::runtime.initialized
            ? linux::gemix::runtime.char_h
            : 16;
        detail::draw_bitmap_text(
            _img,
            _clip,
            text,
            point(p.x, static_cast<coord>(p.y + height)),
            _ink,
            advance,
            height);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }
}
