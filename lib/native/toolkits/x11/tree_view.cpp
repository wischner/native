//
// Implements the Athena classic tree as a themed focusable host.
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
    void tree_view::apply_items() { invalidate(); }
    void tree_view::apply_selection() { invalidate(); }
    void tree_view::apply_expansion(tree_item_id, bool) { invalidate(); }
    void tree_view::apply_scroll_offset() { invalidate(); }

    void tree_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::tree_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }

    void tree_view::show() const {
        auto *binding = linux::x11::tree_view_bindings.object_from_handle(
            const_cast<tree_view *>(this));
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: tree_view is not created.");
        XtManageChild(binding->widget);
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *binding =
            linux::x11::tree_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::tree_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
