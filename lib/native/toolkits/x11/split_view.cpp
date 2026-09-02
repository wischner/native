// Implements an Athena split-view host with native child widgets.

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create() const {
        if (_created) return;
        auto *self = const_cast<split_view *>(this);
        Widget widget = linux::x11::create_collection_host(*self);
        auto *state = new linux::x11::xaw_collection();
        state->widget = widget;
        linux::x11::split_view_bindings.register_pair(self, state);
        _created = true;
        self->refresh_contents();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = linux::x11::split_view_bindings.object_from_handle(
            const_cast<split_view *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("X11/Athena: split_view is not created.");
        XtManageChild(state->widget);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = linux::x11::split_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (state && state->widget) {
            linux::x11::wnd_bindings.unregister_by_handle(state->widget);
            XtDestroyWidget(state->widget);
        }
        linux::x11::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
