//
// Declares shared rendering and pointer routing for disclosure and
// image-collection controls whose backend has no direct native widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

namespace native
{
    class accordion;
    class gpx;
    class icon_view;
    class tree_view;
    class tab_view;

    namespace detail
    {
        // Draw one accordion into a backend-owned graphics context.
        void draw_accordion(accordion &control, gpx &graphics);

        // Draw an emulated accordion at an ancestor-relative origin.
        void draw_accordion_at(accordion &control,
                               gpx &graphics,
                               point origin);

        // Route a client-relative pointer release to an accordion.
        bool handle_accordion_click(accordion &control,
                                    point position);

        // Draw a portable tab view at an ancestor-relative origin.
        void draw_tab_view_at(tab_view &control,
                              gpx &graphics,
                              point origin);

        // Draw one icon view into a backend-owned graphics context.
        void draw_icon_view(icon_view &control, gpx &graphics);

        // Draw an emulated icon view at an ancestor-relative origin.
        void draw_icon_view_at(icon_view &control,
                               gpx &graphics,
                               point origin);

        // Route a client-relative pointer release to an icon view.
        bool handle_icon_view_click(icon_view &control,
                                    point position);

        // Draw one classic tree into a backend-owned graphics context.
        void draw_tree_view(tree_view &control, gpx &graphics);

        // Draw an emulated tree at an ancestor-relative origin.
        void draw_tree_view_at(tree_view &control,
                               gpx &graphics,
                               point origin);

        // Route a client-relative pointer release to a tree.
        bool handle_tree_view_click(tree_view &control,
                                    point position);
    } // namespace detail
} // namespace native
