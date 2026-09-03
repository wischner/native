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
    void canvas::create() const {
        if (_created)
            return;

        auto *self = const_cast<canvas *>(this);
        BView *view = haiku::create_collection_view(*self);
        auto *binding = new haiku::haiku_surface();
        binding->view = view;
        haiku::canvas_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        auto *binding = haiku::canvas_bindings.object_from_handle(
            const_cast<canvas *>(this));
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

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *binding = haiku::canvas_bindings.object_from_handle(self);
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
            haiku::canvas_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
