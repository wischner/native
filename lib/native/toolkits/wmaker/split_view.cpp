// Implements split_view with Window Maker's native WINGs WMSplitView.

#include <algorithm>
#include <cmath>
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

    void constrain(WMSplitView *split,
                   int index,
                   int *minimum,
                   int *maximum) {
        auto *owner = dynamic_cast<native::split_view *>(
            linux::wmaker::wnd_bindings.object_from_handle(
                reinterpret_cast<WMWidget *>(split)));
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !minimum || !maximum)
            return;
        const WMSize dimensions = WMGetViewSize(WMWidgetView(split));
        const int total = WMGetSplitViewVertical(split)
                              ? dimensions.width
                              : dimensions.height;
        const int available = std::max(
            0, total - WMGetSplitViewDividerThickness(split));
        const int first_minimum = std::min<int>(
            available, owner->get_first_minimum());
        const int second_minimum = std::min<int>(
            available - first_minimum,
            owner->get_second_minimum());
        int first = static_cast<int>(std::lround(
            static_cast<double>(available) * owner->get_ratio()));
        first = std::clamp(first,
                           first_minimum,
                           available - second_minimum);
        const int desired = index == 0 ? first : available - first;
        if (state->applying_ratio) {
            *minimum = desired;
            *maximum = desired;
        } else if (index == 0) {
            *minimum = first_minimum;
            *maximum = available - second_minimum;
        } else {
            *minimum = second_minimum;
            *maximum = available - first_minimum;
        }
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
        const Bool vertical =
            get_orientation() == split_orientation::horizontal;
        state->applying_ratio = true;
        // WINGs refreshes its cached native constraints when orientation
        // changes. Cycling the native orientation applies the requested
        // ratio through its supported constrain callback, leaving divider
        // painting, hit testing, and subsequent dragging entirely native.
        WMSetSplitViewVertical(state->split, !vertical);
        WMSetSplitViewVertical(state->split, vertical);
        WMAdjustSplitViewSubviews(state->split);
        state->applying_ratio = false;
    }

    void split_view::apply_minimums() { apply_ratio(); }

    void split_view::apply_splitter_size() {
        // WINGs owns the native divider thickness.
        apply_ratio();
    }

    void split_view::create_native() {
        auto *self = this;
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
        linux::wmaker::wnd_bindings.register_pair(state->split, self);
        linux::wmaker::split_view_bindings.register_pair(self, state);
        WMSetSplitViewConstrainProc(state->split, constrain);
        WMAddSplitViewSubview(state->split, WMWidgetView(state->first));
        WMAddSplitViewSubview(state->split, WMWidgetView(state->second));
        WMCreateEventHandler(WMWidgetView(state->split),
                             ButtonReleaseMask,
                             released,
                             self);
        WMRealizeWidget(state->split);
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
    }

    void split_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || !state->split)
            throw std::runtime_error(
                "Window Maker/WINGs: split_view is not created.");
        WMRealizeWidget(state->split);
        WMMapWidget(state->split);
        WMMapWidget(state->first);
        WMMapWidget(state->second);
        WMRaiseWidget(state->first);
        WMRaiseWidget(state->second);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = binding(*self);
        if (state) {
            linux::wmaker::wnd_bindings.unregister_by_object(self);
            if (state->split) WMDestroyWidget(state->split);
            linux::wmaker::split_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native
