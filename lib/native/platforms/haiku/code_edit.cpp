//
// Implements code_edit in the shared focusable Haiku collection view.
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
    void code_edit::create_native() {
        auto *self = this;
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_collection();
        binding->view = view;
        haiku::code_edit_bindings.register_pair(self, binding);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *binding = haiku::code_edit_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: code_edit is not created.");
        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            haiku::code_edit_bindings.object_from_handle(self);
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
            haiku::code_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
