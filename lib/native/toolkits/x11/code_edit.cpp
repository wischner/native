//
// Implements code_edit in a focusable Athena Form host.
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
    void code_edit::create_native() {
        auto *self = this;
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::code_edit_bindings.register_pair(self, binding);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *binding = linux::x11::code_edit_bindings
                            .object_from_handle(
                                this);
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: code_edit is not created.");
        XtManageChild(binding->widget);
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            linux::x11::code_edit_bindings.object_from_handle(self);
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::code_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
