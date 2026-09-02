//
// Implements code_edit lifecycle in a focusable Motif drawing host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <Xm/Xm.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void code_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state = new linux::openmotif::motif_collection();
        state->widget =
            linux::openmotif::create_collection_host(*self, *state);
        linux::openmotif::code_edit_bindings.register_pair(self, state);
        _created = true;
        self->invalidate();
        self->on_native_create();
    }

    void code_edit::show() const {
        auto *state = linux::openmotif::code_edit_bindings
                          .object_from_handle(
                              const_cast<code_edit *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error(
                "Motif: code_edit is not created.");
        XtManageChild(state->widget);
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state = linux::openmotif::code_edit_bindings
                          .object_from_handle(self);
        linux::openmotif::destroy_collection_host(*self, state);
        linux::openmotif::code_edit_bindings.unregister_by_handle(self);
    }
} // namespace native
