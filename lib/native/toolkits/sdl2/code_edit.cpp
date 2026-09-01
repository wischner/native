//
// Implements code_edit lifecycle in the SDL2 painted-control host.
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
    void code_edit::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: code_edit requires a created parent.");
        auto *self = const_cast<code_edit *>(this);
        linux::sdl2::code_edit_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::code_edits.push_back(self);
        _created = true;
        self->invalidate();
        self->on_wnd_create.emit();
    }

    void code_edit::show() const {
        auto *state = linux::sdl2::code_edit_bindings
                          .object_from_handle(
                              const_cast<code_edit *>(this));
        if (!_created || !state)
            throw std::runtime_error(
                "SDL2: code_edit is not created.");
        state->visible = true;
        invalidate();
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *state = linux::sdl2::code_edit_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::code_edits.erase(
            std::remove(linux::sdl2::code_edits.begin(),
                        linux::sdl2::code_edits.end(),
                        self),
            linux::sdl2::code_edits.end());
        linux::sdl2::code_edit_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
