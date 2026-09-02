//
// Implements the custom Haiku-look wrapping icon grid.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <View.h>
#include <Window.h>

#include <native.h>

#include "collection_view.h"
#include "globals.h"

namespace native
{
    void icon_view::apply_items() { invalidate(); }
    void icon_view::apply_icon_size() { invalidate(); }
    void icon_view::apply_label_mode() { invalidate(); }
    void icon_view::apply_selected_index() { invalidate(); }
    void icon_view::apply_scroll_offset() { invalidate(); }

    void icon_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_collection();
        binding->view = view;
        haiku::icon_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->invalidate();
        self->on_native_create();
    }

    void icon_view::show() const {
        auto *binding = haiku::icon_view_bindings.object_from_handle(
            const_cast<icon_view *>(this));
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: icon_view is not created.");
        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *binding =
            haiku::icon_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            if (binding->view) {
                BWindow *window = binding->view->Window();
                const bool locked = window && window->IsLocked();
                if (window && (locked || window->Lock())) {
                    binding->view->RemoveSelf();
                    delete binding->view;
                    if (!locked)
                        window->Unlock();
                }
            }
            haiku::icon_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
