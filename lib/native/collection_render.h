//
// Declares shared rendering and pointer routing for disclosure and
// image-collection controls whose backend has no direct native widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>

#include <native/geometry.h>

#include "classic_scrollbar.h"

namespace native
{
    class accordion;
    class gpx;
    class icon_view;
    class split_view;
    class tree_view;
    class tab_view;

    namespace detail
    {
        // Shares a collection's painted scrollbar range and hit regions.
        struct collection_scrollbar
        {
            classic_scrollbar_geometry geometry;
            std::uint64_t total = 0;
            std::uint64_t page = 0;
            int step = 1;
        };

        // Resolve the icon-view scrollbar for painting and pointer input.
        collection_scrollbar make_collection_scrollbar(
            const icon_view &control, const theme::metrics &metrics);

        // Resolve the tree scrollbar for painting and pointer input.
        collection_scrollbar make_collection_scrollbar(
            const tree_view &control, const theme::metrics &metrics);

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

        // Draw a portable split view at an ancestor-relative origin.
        void draw_split_view_at(split_view &control,
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
