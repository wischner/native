//
// Implements the SDL2 image-context backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "gpx_img.h"

namespace native
{

    gpx &img::get_gpx() const {
        if (!_gpx) {
            _gpx = std::make_unique<gpx_img>(*this);
        }
        return *_gpx;
    }

} // namespace native
