//
// Implements table_view with a focusable Athena Form and the shared
// Xaw-resource-aware virtual table painter.
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
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<table_view *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::table_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }

    void table_view::show() const {
        auto *binding = linux::x11::table_view_bindings
                            .object_from_handle(
                                const_cast<table_view *>(this));
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: table_view is not created.");
        XtManageChild(binding->widget);
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *binding = linux::x11::table_view_bindings
                            .object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::table_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
