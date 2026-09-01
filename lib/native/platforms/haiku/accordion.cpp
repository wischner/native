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

    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_collection();
        binding->view = view;
        haiku::accordion_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_wnd_create.emit();
    }

    void accordion::show() const {
        auto *binding = haiku::accordion_bindings.object_from_handle(
            const_cast<accordion *>(this));
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

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *binding =
            haiku::accordion_bindings.object_from_handle(self);
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
            haiku::accordion_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
