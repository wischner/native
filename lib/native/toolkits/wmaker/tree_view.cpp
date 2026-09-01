//
// Implements Window Maker tree_view lifecycle through a native WINGs
// frame; shared semantic painting supplies the classic outline rows.
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
    void tree_view::apply_items() { invalidate(); }
    void tree_view::apply_selection() { invalidate(); }
    void tree_view::apply_expansion(tree_item_id, bool) { invalidate(); }
    void tree_view::apply_scroll_offset() { invalidate(); }

    void tree_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *state = linux::wmaker::create_collection_frame(*self);
        linux::wmaker::tree_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void tree_view::show() const {
        auto *state = linux::wmaker::tree_view_bindings
                          .object_from_handle(
                              const_cast<tree_view *>(this));
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: tree_view is not created.");
        WMMapWidget(state->frame);
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *state = linux::wmaker::tree_view_bindings
                          .object_from_handle(self);
        linux::wmaker::destroy_collection_frame(*self, state);
        linux::wmaker::tree_view_bindings.unregister_by_handle(self);
    }
} // namespace native
