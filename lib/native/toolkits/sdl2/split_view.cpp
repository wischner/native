//
// Implements the portable splitter in the SDL structural backend.
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
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: split_view requires a created parent.");
        auto *self = this;
        linux::sdl2::split_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::split_views.push_back(self);
        self->refresh_contents();
    }

    void split_view::show_native() {
        auto *state = linux::sdl2::split_view_bindings.object_from_handle(
            this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: split_view is not created.");
        state->visible = true;
        get_first().show();
        get_second().show();
        invalidate();
    }

    void split_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::sdl2::split_view_bindings
                          .object_from_handle(self);
        linux::sdl2::split_views.erase(
            std::remove(linux::sdl2::split_views.begin(),
                        linux::sdl2::split_views.end(), self),
            linux::sdl2::split_views.end());
        linux::sdl2::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
