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
    void code_edit::create_native() {
        auto *self = this;
        auto *state = new linux::openmotif::motif_collection();
        state->widget =
            linux::openmotif::create_collection_host(*self, *state);
        linux::openmotif::code_edit_bindings.register_pair(self, state);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *state = linux::openmotif::code_edit_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->widget)
            throw std::runtime_error(
                "Motif: code_edit is not created.");
        XtManageChild(state->widget);
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::openmotif::code_edit_bindings
                          .object_from_handle(self);
        linux::openmotif::destroy_collection_host(*self, state);
        linux::openmotif::code_edit_bindings.unregister_by_handle(self);
    }
} // namespace native
