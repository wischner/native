// Implements a two-pane XView split container.

#include <stdexcept>

#include <native.h>
#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    linux::openlook::openlook_split_view *binding(
        native::split_view &owner) {
        return linux::openlook::split_view_bindings
            .object_from_handle(&owner);
    }

    void position(Panel panel, const native::rect &bounds) {
        if (!panel) return;
        xv_set(panel,
               XV_X, bounds.p.x,
               XV_Y, bounds.p.y,
               XV_WIDTH, bounds.d.w,
               XV_HEIGHT, bounds.d.h,
               nullptr);
    }

    Notify_value handle_splitter_event(
        Panel panel,
        Event *event,
        Notify_arg argument,
        Notify_event_type type) {
        auto *owner = reinterpret_cast<native::split_view *>(
            xv_get(panel, WIN_CLIENT_DATA));
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !event) {
            return notify_next_event_func(
                panel,
                reinterpret_cast<Notify_event>(event),
                argument,
                type);
        }

        const native::point position(event_x(event), event_y(event));
        const int action = event_action(event);
        if (action == ACTION_SELECT && event_is_down(event) &&
            owner->get_splitter_bounds().contains(position)) {
            state->dragging = true;
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                position));
        } else if (action == LOC_DRAG && state->dragging) {
            owner->on_native_mouse_move(position);
        } else if (action == ACTION_SELECT &&
                   !event_is_down(event) && state->dragging) {
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                position));
            state->dragging = false;
        }
        return notify_next_event_func(
            panel,
            reinterpret_cast<Notify_event>(event),
            argument,
            type);
    }
}

namespace native
{
    void split_view::apply_orientation() { apply_ratio(); }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state) return;
        position(state->first, get_first_bounds());
        position(state->second, get_second_bounds());
    }

    void split_view::apply_minimums() { apply_ratio(); }
    void split_view::apply_splitter_size() { apply_ratio(); }

    void split_view::create_native() {
        auto *self = this;
        Panel parent = linux::openlook::parent_panel(self);
        auto *state = new linux::openlook::openlook_split_view();
        state->host = static_cast<Panel>(xv_create(
            parent, PANEL,
            PANEL_BORDER, FALSE,
            XV_X, _bounds.p.x,
            XV_Y, _bounds.p.y,
            XV_WIDTH, _bounds.d.w,
            XV_HEIGHT, _bounds.d.h,
            XV_SHOW, FALSE,
            nullptr));
        if (state->host) {
            state->first = static_cast<Panel>(xv_create(
                state->host, PANEL, PANEL_BORDER, FALSE,
                XV_SHOW, FALSE, nullptr));
            state->second = static_cast<Panel>(xv_create(
                state->host, PANEL, PANEL_BORDER, FALSE,
                XV_SHOW, FALSE, nullptr));
        }
        if (!state->host || !state->first || !state->second) {
            if (state->host) xv_destroy_safe(state->host);
            delete state;
            throw std::runtime_error(
                "OpenLook/XView: failed to create split view.");
        }
        xv_set(state->host,
               WIN_CLIENT_DATA,
               self,
               WIN_NOTIFY_SAFE_EVENT_PROC,
               handle_splitter_event,
               WIN_CONSUME_EVENTS,
               LOC_DRAG,
               ACTION_SELECT,
               nullptr,
               nullptr);
        linux::openlook::wnd_bindings.register_pair(state->host, self);
        linux::openlook::split_view_bindings.register_pair(self, state);
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
    }

    void split_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state)
            throw std::runtime_error(
                "OpenLook/XView: split_view is not created.");
        xv_set(state->host, XV_SHOW, TRUE, nullptr);
        xv_set(state->first, XV_SHOW, TRUE, nullptr);
        xv_set(state->second, XV_SHOW, TRUE, nullptr);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = binding(*self);
        if (state && state->host) {
            linux::openlook::wnd_bindings.unregister_by_handle(state->host);
            xv_destroy_safe(state->host);
        }
        linux::openlook::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
