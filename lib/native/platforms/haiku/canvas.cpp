//
// Implements the Haiku paintable child surface. The shared collection
// host view owns the drawing, resize, and pointer routing; the
// portable canvas owns everything drawn into it.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <View.h>
#include <Window.h>

#include <native.h>
#include <native/canvas.h>

#include "collection_view.h"
#include "globals.h"

namespace native
{
    void canvas::create_native() {
        auto *self = this;
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_surface();
        binding->view = view;
        haiku::canvas_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        self->relayout_children();
    }

    void canvas::show_native() {
        auto *binding = haiku::canvas_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: canvas is not created.");

        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *binding = haiku::canvas_bindings.object_from_handle(self);
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
            haiku::canvas_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
