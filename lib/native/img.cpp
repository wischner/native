//
// Implements owned RGBA image storage and access.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native/graphics.h>

#include "gpx_img.h"

namespace native
{
    gpx_img::~gpx_img() = default;

    img::img(dim w, dim h)
        : _w(w), _h(h), _data(std::make_unique<rgba[]>(_w * _h)) {
        if (w == 0 || h == 0)
            throw std::invalid_argument("img: dimensions must be > 0");
    }

    img::~img() = default;

    coord img::w() const {
        return _w;
    }

    coord img::h() const {
        return _h;
    }

    rgba *img::pixels() {
        return _data.get();
    }

    const rgba *img::pixels() const {
        return _data.get();
    }

} // namespace native
