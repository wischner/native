//
// Implements the Athena icon view as a themed, focusable grid host.
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
    void icon_view::apply_items() { invalidate(); }
    void icon_view::apply_icon_size() { invalidate(); }
    void icon_view::apply_label_mode() { invalidate(); }
    void icon_view::apply_selected_index() { invalidate(); }
    void icon_view::apply_scroll_offset() { invalidate(); }

    void icon_view::create_native() {
        auto *self = this;
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::icon_view_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
    }

    void icon_view::show_native() {
        auto *binding = linux::x11::icon_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: icon_view is not created.");
        XtManageChild(binding->widget);
    }

    void icon_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            linux::x11::icon_view_bindings.object_from_handle(self);
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::icon_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
