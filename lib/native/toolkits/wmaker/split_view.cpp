// Implements split_view with Window Maker's native WINGs WMSplitView.

#include <algorithm>
#include <stdexcept>

#include <WINGs/WINGs.h>
#include <native.h>

#include "globals.h"

namespace
{
    linux::wmaker::native_split_view *binding(native::split_view &owner) {
        return linux::wmaker::split_view_bindings.object_from_handle(&owner);
    }

    void released(XEvent *event, void *data) {
        if (!event || event->type != ButtonRelease) return;
        auto *owner = static_cast<native::split_view *>(data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !state->first || !state->split) return;
        const WMSize first = WMGetViewSize(WMWidgetView(state->first));
        const native::size total = owner->get_dimensions();
        const int divider = WMGetSplitViewDividerThickness(state->split);
        const int available = std::max(
            1,
            (owner->get_orientation() == native::split_orientation::horizontal
                 ? static_cast<int>(total.w)
                 : static_cast<int>(total.h)) - divider);
        const int extent = owner->get_orientation() ==
                                   native::split_orientation::horizontal
                               ? first.width
                               : first.height;
        owner->on_native_ratio(static_cast<float>(extent) / available);
    }
}

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (!state || !state->split) return;
        WMSetSplitViewVertical(
            state->split,
            get_orientation() == split_orientation::horizontal);
        apply_ratio();
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->split || !state->first || !state->second)
            return;
        const rect first = get_first_bounds();
        const rect second = get_second_bounds();
        WMResizeWidget(state->first, first.d.w, first.d.h);
        WMResizeWidget(state->second, second.d.w, second.d.h);
        WMAdjustSplitViewSubviews(state->split);
    }

    void split_view::apply_minimums() { apply_ratio(); }

    void split_view::apply_splitter_size() {
        // WINGs owns the native divider thickness.
        apply_ratio();
    }

    void split_view::create() const {
        if (_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = new linux::wmaker::native_split_view();
        state->split = WMCreateSplitView(
            linux::wmaker::parent_widget(self));
        if (!state->split) {
            delete state;
            throw std::runtime_error(
                "Window Maker/WINGs: failed to create WMSplitView.");
        }
        const point position = linux::wmaker::control_position(self);
        WMMoveWidget(state->split, position.x, position.y);
        WMResizeWidget(state->split, _bounds.d.w, _bounds.d.h);
        WMSetSplitViewVertical(
            state->split,
            get_orientation() == split_orientation::horizontal);
        state->first = WMCreateFrame(state->split);
        state->second = WMCreateFrame(state->split);
        WMSetFrameRelief(state->first, WRFlat);
        WMSetFrameRelief(state->second, WRFlat);
        WMAddSplitViewSubview(state->split, WMWidgetView(state->first));
        WMAddSplitViewSubview(state->split, WMWidgetView(state->second));
        WMCreateEventHandler(WMWidgetView(state->split),
                             ButtonReleaseMask,
                             released,
                             self);
        linux::wmaker::wnd_bindings.register_pair(state->split, self);
        linux::wmaker::split_view_bindings.register_pair(self, state);
        WMRealizeWidget(state->split);
        _created = true;
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state || !state->split)
            throw std::runtime_error(
                "Window Maker/WINGs: split_view is not created.");
        WMRealizeWidget(state->split);
        WMMapWidget(state->split);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state) {
            linux::wmaker::wnd_bindings.unregister_by_object(self);
            if (state->split) WMDestroyWidget(state->split);
            linux::wmaker::split_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native
