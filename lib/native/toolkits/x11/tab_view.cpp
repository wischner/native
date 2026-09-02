//
// Implements portable tabs in an Athena drawable host.
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
    void tab_view::apply_items() { invalidate(); }
    void tab_view::apply_selected_index() { invalidate(); }

    void tab_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *state = new linux::x11::xaw_collection();
        state->widget = widget;
        linux::x11::tab_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *state = linux::x11::tab_view_bindings.object_from_handle(
            const_cast<tab_view *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("X11/Athena: tab_view is not created.");
        XtManageChild(state->widget);
        if (XtIsRealized(state->widget))
            XRaiseWindow(linux::x11::cached_display, XtWindow(state->widget));
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created())
                content.show();
        }
    }

    void tab_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = linux::x11::tab_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (state) {
            if (state->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(state->widget);
                XtDestroyWidget(state->widget);
            }
            linux::x11::tab_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native
