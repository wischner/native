//
// Declares shared native-theme table rendering and pointer routing for
// backends without a complete multi-column native widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

namespace native
{
    class gpx;
    class table_view;
}

namespace native::detail
{
    // Draw a complete table in its own client coordinate space.
    void draw_table_view(table_view &control, gpx &graphics);

    // Draw a table into a parent graphics context at an offset.
    void draw_table_view_at(table_view &control,
                            gpx &graphics,
                            point origin);

    // Route one client-relative primary-button release.
    bool handle_table_click(table_view &control, point position);
} // namespace native::detail
