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
    void code_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state =
            linux::openlook::create_collection_panel(*self);
        linux::openlook::code_edit_bindings.register_pair(self, state);
        _created = true;
        self->invalidate();
        self->on_wnd_create.emit();
    }

    void code_edit::show() const {
        auto *state = linux::openlook::code_edit_bindings
                          .object_from_handle(
                              const_cast<code_edit *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: code_edit is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state = linux::openlook::code_edit_bindings
                          .object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::code_edit_bindings.unregister_by_handle(self);
    }
} // namespace native
