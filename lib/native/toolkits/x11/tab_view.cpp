//
// Implements portable tabs in an Athena drawable host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/List.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void tab_view::apply_items() { invalidate(); }
    void tab_view::apply_selected_index() {
        const int selected = get_selected_index();
        if (selected >= 0) {
            auto &content = get_item(selected).get_content();
            Widget page = linux::x11::wnd_bindings.handle_from_object(&content);
            // A List used as a page must not close the selected tab's join
            // with a second, independently drawn native enclosure.
            if (page && XtIsSubclass(page, listWidgetClass)) {
                XtVaSetValues(page, XtNborderWidth, 0, nullptr);
                content.set_bounds(get_content_bounds());
            }
        }
        invalidate();
    }

    void tab_view::create_native() {
        auto *self = this;
        Widget widget = linux::x11::create_collection_host(*self);
        auto *state = new linux::x11::xaw_collection();
        state->widget = widget;
        linux::x11::tab_view_bindings.register_pair(self, state);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void tab_view::show_native() {
        auto *state = linux::x11::tab_view_bindings.object_from_handle(
            this);
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

    void tab_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::x11::tab_view_bindings.object_from_handle(self);
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
