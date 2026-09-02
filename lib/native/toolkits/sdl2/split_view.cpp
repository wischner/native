// Implements the portable splitter in the SDL structural backend.

#include <stdexcept>
#include <native.h>
#include "globals.h"

namespace native
{
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create() const {
        if (_created) return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error("SDL2: split_view requires a created parent.");
        auto *self = const_cast<split_view *>(this);
        linux::sdl2::split_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        _created = true;
        self->refresh_contents();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = linux::sdl2::split_view_bindings.object_from_handle(
            const_cast<split_view *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: split_view is not created.");
        state->visible = true;
        get_first().show();
        get_second().show();
        invalidate();
    }

    void split_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = linux::sdl2::split_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
