//
// Declares shared painting for source editors hosted by every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

namespace native
{
    class code_edit;
    class gpx;

    // Paint one editor at an ancestor-relative origin.
    void draw_code_edit(code_edit &control,
                        gpx &graphics,
                        point origin = point());
} // namespace native
