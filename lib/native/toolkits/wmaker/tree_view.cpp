//
// Implements Window Maker tree_view lifecycle through a native WINGs
// frame; shared semantic painting supplies the classic outline rows.
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
    void tree_view::apply_items() { invalidate(); }
    void tree_view::apply_selection() { invalidate(); }
    void tree_view::apply_expansion(tree_item_id, bool) { invalidate(); }
    void tree_view::apply_scroll_offset() { invalidate(); }

    void tree_view::create_native() {
        auto *self = this;
        auto *state = linux::wmaker::create_collection_frame(*self);
        linux::wmaker::tree_view_bindings.register_pair(self, state);
        self->synchronize_theme_metrics();
    }

    void tree_view::show_native() {
        auto *state = linux::wmaker::tree_view_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: tree_view is not created.");
        WMRealizeWidget(state->frame);
        WMMapWidget(state->frame);
    }

    void tree_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::wmaker::tree_view_bindings
                          .object_from_handle(self);
        linux::wmaker::destroy_collection_frame(*self, state);
        linux::wmaker::tree_view_bindings.unregister_by_handle(self);
    }
} // namespace native
