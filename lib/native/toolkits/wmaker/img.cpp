//
// Connects owned RGBA images to the Window Maker image graphics
// context.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <memory>

#include <native/graphics.h>

#include "../../gpx_img.h"

namespace native
{
    gpx &img::get_gpx() const {
        if (!_gpx)
            _gpx = std::make_unique<gpx_img>(*this);
        return *_gpx;
    }
} // namespace native
