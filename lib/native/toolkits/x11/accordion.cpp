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

    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::accordion_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void accordion::show() const {
        auto *binding = linux::x11::accordion_bindings.object_from_handle(
            const_cast<accordion *>(this));
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: accordion is not created.");
        XtManageChild(binding->widget);
        if (XtIsRealized(binding->widget)) {
            XRaiseWindow(linux::x11::cached_display,
                         XtWindow(binding->widget));
        }
    }

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *binding =
            linux::x11::accordion_bindings.object_from_handle(self);
        self->on_native_destroy();
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
