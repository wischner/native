//
// Implements hierarchy walking shared by emulated child-control
// backends. Coordinates stay in the top-level window's client space.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "emulated_tree.h"

#include <native/wnd.h>

namespace native::detail
{
    wnd *root_of(wnd *control) {
        while (control && control->get_parent())
            control = control->get_parent();
        return control;
    }

    point origin_in_root(const wnd &control) {
        int x = control.get_position().x;
        int y = control.get_position().y;
        for (wnd *parent = control.get_parent();
             parent && parent->get_parent();
             parent = parent->get_parent()) {
            x += parent->get_position().x;
            y += parent->get_position().y;
        }
        return point(static_cast<coord>(x),
                     static_cast<coord>(y));
    }

    rect root_bounds(const wnd &control) {
        return rect(origin_in_root(control), control.get_dimensions());
    }

    wnd *deepest_at(wnd &root, point position) {
        wnd *result = &root;
        for (wnd *child : root._children) {
            if (!child || !child->get_created() ||
                !child->get_visible() ||
                !root_bounds(*child).contains(position))
                continue;
            if (wnd *candidate = deepest_at(*child, position))
                result = candidate;
        }
        return result;
    }
} // namespace native::detail
