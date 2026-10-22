//
// Supplies the same attributed navigation artwork to every Vision backend.
// Images are embedded PNGs, independent of installed fonts and file icons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>

namespace vision
{
    enum class navigation_icon { settings, calendar, clock };

    // Decode one Font Awesome Free tile into an ordinary Native image.
    std::shared_ptr<const native::img> navigation_image(navigation_icon icon);
}
