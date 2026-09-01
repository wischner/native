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

    void icon_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *binding = new linux::x11::xaw_collection();
        binding->widget = widget;
        linux::x11::icon_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void icon_view::show() const {
        auto *binding = linux::x11::icon_view_bindings.object_from_handle(
            const_cast<icon_view *>(this));
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: icon_view is not created.");
        XtManageChild(binding->widget);
    }

    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *binding =
            linux::x11::icon_view_bindings.object_from_handle(self);
        self->on_native_destroy();
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
