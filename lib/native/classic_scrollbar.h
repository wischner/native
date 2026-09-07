//
// Declares the shared geometry and painting used by library-owned classic
// scrollbars. Backends can retain native input routing while presenting the
// same arrow buttons, trough, and thumb in every painted control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>

#include <native/theme.h>

namespace native::detail
{
    // Stores the complete geometry of one classic scrollbar.
    struct classic_scrollbar_geometry
    {
        rect bounds;
        rect decrement;
        rect increment;
        rect trough;
        rect thumb;
    };

    // Stores the parts of a classic scrollbar that a scroll range
    // cannot move: both arrow buttons and the thumb's trough.
    struct classic_scrollbar_edges
    {
        rect decrement;
        rect increment;
        rect trough;
    };

    //
    // Split a scrollbar rectangle into arrows and the thumb trough.
    //
    // Notes:
    //      Each square arrow button is as long as the bar is thick,
    //      and never longer than half the bar, so a short bar keeps
    //      both buttons and loses only its trough.
    //
    classic_scrollbar_edges make_classic_scrollbar_edges(
        const rect &bounds,
        scrollbar_orientation orientation);

    // Resolve arrow, trough, and thumb rectangles from a scroll range.
    classic_scrollbar_geometry make_classic_scrollbar(
        const rect &bounds,
        scrollbar_orientation orientation,
        std::uint64_t total,
        std::uint64_t page,
        std::uint64_t value,
        int minimum_thumb);

    // Convert a dragged thumb coordinate back to its clamped scroll value.
    std::uint64_t classic_scrollbar_drag_value(
        const classic_scrollbar_geometry &scrollbar,
        scrollbar_orientation orientation,
        int pointer_coordinate,
        int grab_offset,
        std::uint64_t total,
        std::uint64_t page);

    // Paint every part of a classic scrollbar with one theme.
    void draw_classic_scrollbar(
        theme &appearance,
        scrollbar_orientation orientation,
        const rect &bounds,
        const rect &thumb,
        const theme::state &state);
} // namespace native::detail
