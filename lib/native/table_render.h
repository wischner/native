//
// Declares shared native-theme table rendering and pointer routing for
// backends without a complete multi-column native widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>
#include <native/theme.h>

namespace native
{
    class gpx;
    class table_view;
}

namespace native::detail
{
    // Return the body shared by painting, paging, and native scroller hosts.
    rect table_body_bounds(const table_view &control,
                           const theme::metrics &metrics);

    // Draw a complete table in its own client coordinate space.
    void draw_table_view(table_view &control, gpx &graphics);

    // Draw a table into a parent graphics context at an offset.
    void draw_table_view_at(table_view &control,
                            gpx &graphics,
                            point origin);

    // Begin dragging a portable table scrollbar thumb. Return false when the
    // point is not over either thumb; horizontal identifies the captured bar.
    bool begin_table_scrollbar_drag(table_view &control,
                                    point position,
                                    bool &horizontal,
                                    int &grab_offset);

    // Move a previously captured portable table scrollbar thumb.
    bool drag_table_scrollbar(table_view &control,
                              point position,
                              bool horizontal,
                              int grab_offset);

    // Route one client-relative primary-button release.
    bool handle_table_click(table_view &control, point position);
} // namespace native::detail
