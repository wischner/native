//
// Implements the Athena accordion as a themed, focusable Form host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void accordion::apply_items() {
        invalidate();
    }

    void accordion::create_native() {
        auto *self = this;
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::accordion_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void accordion::show_native() {
        auto *binding = linux::x11::accordion_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: accordion is not created.");
        XtManageChild(binding->widget);
        if (XtIsRealized(binding->widget)) {
            XRaiseWindow(linux::x11::cached_display,
                         XtWindow(binding->widget));
        }
    }

    void accordion::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            linux::x11::accordion_bindings.object_from_handle(self);
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::accordion_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
