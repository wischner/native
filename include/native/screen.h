//
// Declares display detection and virtual-desktop geometry queries.
// Each backend populates the shared screen collection from native APIs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <vector>

#include "geometry.h"

namespace native
{
    // Describes one physical display and its usable work area.
    class screen final
    {
    public:
        // Construct a detected display description.
        screen(
            int index,
            const rect &bounds,
            const rect &work_area,
            bool is_primary);

        // Return the backend enumeration index.
        int index() const;

        // Determine whether this is the primary display.
        bool is_primary() const;

        // Determine whether width is at least as large as height.
        bool is_landscape() const;

        // Return full bounds in virtual-desktop coordinates.
        const rect &bounds() const;

        // Return the usable bounds after excluding system UI.
        const rect &work_area() const;

        //
        // Refresh and return all detected screens.
        //
        // Returns:
        //      Process-owned data valid until the next detect().
        //
        // Throws:
        //      std::runtime_error when native display detection fails.
        //
        static const std::vector<screen> &detect();

        // Return the number of screens from the latest detection.
        static int count();

        //
        // Return a screen by enumeration index.
        //
        // Returns:
        //      A process-owned screen, or null for an invalid index.
        //
        static screen *at(int index);

        // Return the primary screen, or null when none was detected.
        static screen *primary();

        // Return the union of all detected display bounds.
        static rect virtual_bounds();

    private:
        int _index;
        rect _bounds;
        rect _work_area;
        bool _is_primary;

        // Normalize indexes and select exactly one primary screen.
        static void normalize();

        static std::vector<screen> _screens;
    };
}
