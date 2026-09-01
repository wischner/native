//
// Implements code_edit lifecycle in a focusable WINGs frame host.
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
    void code_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state =
            linux::wmaker::create_collection_frame(*self);
        linux::wmaker::code_edit_bindings.register_pair(self, state);
        _created = true;
        self->invalidate();
        self->on_wnd_create.emit();
    }

    void code_edit::show() const {
        auto *state = linux::wmaker::code_edit_bindings
                          .object_from_handle(
                              const_cast<code_edit *>(this));
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: code_edit is not created.");
        WMMapWidget(state->frame);
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state = linux::wmaker::code_edit_bindings
                          .object_from_handle(self);
        linux::wmaker::destroy_collection_frame(*self, state);
        linux::wmaker::code_edit_bindings.unregister_by_handle(self);
    }
} // namespace native
