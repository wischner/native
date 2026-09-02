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
    void code_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::code_edit_bindings.register_pair(self, binding);
        _created = true;
        self->invalidate();
        self->on_native_create();
    }

    void code_edit::show() const {
        auto *binding = linux::x11::code_edit_bindings
                            .object_from_handle(
                                const_cast<code_edit *>(this));
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: code_edit is not created.");
        XtManageChild(binding->widget);
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        auto *binding =
            linux::x11::code_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
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
