//
// Implements table_view as an SDL2-hosted native-theme virtual table.
// Rows remain model-backed and are painted only for the viewport.
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
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: table_view requires a created parent.");
        auto *self = const_cast<table_view *>(this);
        linux::sdl2::table_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::table_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }

    void table_view::show() const {
        auto *state = linux::sdl2::table_view_bindings
                          .object_from_handle(
                              const_cast<table_view *>(this));
        if (!_created || !state)
            throw std::runtime_error(
                "SDL2: table_view is not created.");
        state->visible = true;
        invalidate();
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *state = linux::sdl2::table_view_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::table_views.erase(
            std::remove(linux::sdl2::table_views.begin(),
                        linux::sdl2::table_views.end(), self),
            linux::sdl2::table_views.end());
        linux::sdl2::table_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
