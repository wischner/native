//
// Implements table_view in an XView Panel host; the shared painter
// uses OLGX and the active Panel color map for OPEN LOOK appearance.
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
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *state =
            linux::openlook::create_collection_panel(*self);
        linux::openlook::table_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }

    void table_view::show() const {
        auto *state = linux::openlook::table_view_bindings
                          .object_from_handle(
                              const_cast<table_view *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: table_view is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *state = linux::openlook::table_view_bindings
                          .object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::table_view_bindings.unregister_by_handle(self);
    }
} // namespace native
