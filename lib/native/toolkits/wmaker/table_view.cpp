//
// Implements table_view with a WINGs relief frame and Window Maker
// fonts, colors, focus, and selection rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create_native() {
        auto *self = this;
        auto *state = linux::wmaker::create_collection_frame(*self);
        linux::wmaker::table_view_bindings.register_pair(self, state);
        self->synchronize_theme_metrics();
    }

    void table_view::show_native() {
        auto *state = linux::wmaker::table_view_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: table_view is not created.");
        WMRealizeWidget(state->frame);
        WMMapWidget(state->frame);
    }

    void table_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::wmaker::table_view_bindings
                          .object_from_handle(self);
        linux::wmaker::destroy_collection_frame(*self, state);
        linux::wmaker::table_view_bindings.unregister_by_handle(self);
    }
} // namespace native
