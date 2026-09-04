//
// Declares shared coordinate and hit-testing helpers for backends whose
// child controls are regions of one emulated top-level surface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

namespace native
{
    class wnd;

    namespace detail
    {
        // Return the top-level window an emulated control belongs to.
        wnd *root_of(wnd *control);

        // Return a control origin in its root window's coordinates.
        point origin_in_root(const wnd &control);

        // Return control bounds in its root window's coordinates.
        rect root_bounds(const wnd &control);

        // Return the deepest created descendant containing a root point.
        wnd *deepest_at(wnd &root, point position);
    } // namespace detail
} // namespace native
