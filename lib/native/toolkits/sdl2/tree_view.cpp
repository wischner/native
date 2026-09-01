//
// Implements SDL2 tree_view lifecycle through the toolkit-owned
// collection registry while shared code handles painting and input.
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
    void tree_view::apply_items() { invalidate(); }
    void tree_view::apply_selection() { invalidate(); }
    void tree_view::apply_expansion(tree_item_id, bool) { invalidate(); }
    void tree_view::apply_scroll_offset() { invalidate(); }

    void tree_view::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: tree_view requires a created parent.");
        auto *self = const_cast<tree_view *>(this);
        linux::sdl2::tree_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::tree_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void tree_view::show() const {
        auto *state = linux::sdl2::tree_view_bindings
                          .object_from_handle(
                              const_cast<tree_view *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: tree_view is not created.");
        state->visible = true;
        invalidate();
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *state = linux::sdl2::tree_view_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::tree_views.erase(
            std::remove(linux::sdl2::tree_views.begin(),
                        linux::sdl2::tree_views.end(),
                        self),
            linux::sdl2::tree_views.end());
        linux::sdl2::tree_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
