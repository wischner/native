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
    void code_edit::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: code_edit requires a created parent.");
        auto *self = this;
        linux::sdl2::code_edit_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::code_edits.push_back(self);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *state = linux::sdl2::code_edit_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state)
            throw std::runtime_error(
                "SDL2: code_edit is not created.");
        state->visible = true;
        invalidate();
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::sdl2::code_edit_bindings
                          .object_from_handle(self);
        linux::sdl2::code_edits.erase(
            std::remove(linux::sdl2::code_edits.begin(),
                        linux::sdl2::code_edits.end(),
                        self),
            linux::sdl2::code_edits.end());
        linux::sdl2::code_edit_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
