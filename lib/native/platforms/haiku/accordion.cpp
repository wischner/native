//
// Implements the custom Haiku-look accordion host.
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
    void accordion::apply_items() { invalidate(); }

    void accordion::create_native() {
        auto *self = this;
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_collection();
        binding->view = view;
        haiku::accordion_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void accordion::show_native() {
        auto *binding = haiku::accordion_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: accordion is not created.");
        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void accordion::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            haiku::accordion_bindings.object_from_handle(self);
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
            haiku::accordion_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
