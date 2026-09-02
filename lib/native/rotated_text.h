//
// Declares backend-neutral rotated text rendering for native controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/graphics.h>

namespace native::detail
{
    // Draw centered text rotated by one quarter turn inside bounds.
    void draw_rotated_text(gpx &graphics,
                           const std::string &text,
                           const rect &bounds,
                           bool clockwise,
                           int padding = 0);
} // namespace native::detail
