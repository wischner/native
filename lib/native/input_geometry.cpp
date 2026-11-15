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
#include <native/property_grid.h>
#include <native/date_picker.h>
#include <native/time_picker.h>
#include <native/settings_sidebar.h>
#include "wnd_peer.h"
#include <algorithm>
#include <limits>

namespace native::detail
{
    // Preserve compact dimensions throughout composed input hosts.
    static bool form_control(const wnd &owner) {
        for (auto *current = &owner; current; current = current->get_parent()) {
            if (dynamic_cast<const property_grid *>(current) ||
                dynamic_cast<const settings_sidebar *>(current) ||
                wnd_peer_access::is_non_client_surface(*current)) return false;
        }
        return true;
    }

    rect input_bounds(const wnd &owner, const rect &requested) {
        if (!requested.h()) return requested;
        auto *root = owner.get_parent();
        if (!root || !root->get_created()) return requested;
        while (root->get_parent()) root = root->get_parent();
        if (!root->get_created()) return requested;
        const auto metrics = theme::create(root->get_gpx())->defaults();
        const int height = std::clamp(
            metrics.form_control_height > 0 && form_control(owner)
                ? metrics.form_control_height : metrics.text_edit_height,
            1, int(std::numeric_limits<dim>::max()));
        const int y = requested.y1() +
            std::max(0, int(requested.h()) - height) / 2;
        return rect(requested.x1(), std::min(y, 32767), requested.w(), height);
    }

    rect button_bounds(const wnd &owner, const rect &requested) {
        if (requested.h() != 28 || !form_control(owner)) return requested;
        auto *root = owner.get_parent();
        if (!root || !root->get_created()) return requested;
        while (root->get_parent()) root = root->get_parent();
        const int height = theme::create(root->get_gpx())->get_form_control_height();
        if (height <= 0) return requested;
        const int y = requested.y1() + std::max(0, 28 - height) / 2;
        return rect(requested.x1(), std::min(y, 32767),
                    requested.w(), std::min(height, 65535));
    }
}
