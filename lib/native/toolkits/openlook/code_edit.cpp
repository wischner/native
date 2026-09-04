//
// Implements code_edit lifecycle in a focusable XView Panel host.
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
    void code_edit::create_native() {
        auto *self = this;
        auto *state =
            linux::openlook::create_collection_panel(*self);
        linux::openlook::code_edit_bindings.register_pair(self, state);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *state = linux::openlook::code_edit_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: code_edit is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::openlook::code_edit_bindings
                          .object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::code_edit_bindings.unregister_by_handle(self);
    }
} // namespace native
