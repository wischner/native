//
// Implements OPEN LOOK tree_view lifecycle through a native XView
// Panel host; OLGX-aware shared painting supplies the classic tree.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>

#include <xview/xview.h>

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
        auto *state =
            linux::openlook::create_collection_panel(*self);
        linux::openlook::tree_view_bindings.register_pair(self, state);
        self->synchronize_theme_metrics();
    }

    void tree_view::show_native() {
        auto *state = linux::openlook::tree_view_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: tree_view is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }

    void tree_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::openlook::tree_view_bindings
                          .object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::tree_view_bindings.unregister_by_handle(self);
    }
} // namespace native
