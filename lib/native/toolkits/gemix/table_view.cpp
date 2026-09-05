//
// Implements table_view through GEMix AES/VDI-native theme drawing
// with compact virtual row mapping.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native.h>

#include "globals.h"

namespace native
{
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: table_view requires a created parent.");
        auto *self = this;
        linux::gemix::table_views.push_back(self);
        self->synchronize_theme_metrics();
    }

    void table_view::show_native() {
        if (!_created)
            throw std::runtime_error(
                "GEMix: table_view is not created.");
        invalidate();
    }

    void table_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        linux::gemix::forget_drag(self);
        linux::gemix::table_views.erase(
            std::remove(linux::gemix::table_views.begin(),
                        linux::gemix::table_views.end(), self),
            linux::gemix::table_views.end());
    }
} // namespace native
