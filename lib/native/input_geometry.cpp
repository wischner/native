//
// Shares the theme-derived fixed height of entries, combos and pickers.
// Geometry is resolved only when a created ancestor can supply its theme.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "input_geometry.h"
#include <native/wnd.h>
#include <native/theme.h>
#include <algorithm>
#include <limits>

namespace native::detail
{
    rect input_bounds(const wnd &owner, const rect &requested) {
        if (!requested.h()) return requested;
        auto *root = owner.get_parent();
        if (!root || !root->get_created()) return requested;
        while (root->get_parent()) root = root->get_parent();
        if (!root->get_created()) return requested;
        const int height = std::clamp(
            theme::create(root->get_gpx())->get_text_edit_height(),
            1, int(std::numeric_limits<dim>::max()));
        const int y = requested.y1() +
            std::max(0, int(requested.h()) - height) / 2;
        return rect(requested.x1(), std::min(y, 32767), requested.w(), height);
    }
}
