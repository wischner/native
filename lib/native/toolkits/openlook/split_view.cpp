//
// Implements native XView pane hosts and captured divider dragging.
// The divider grip is ordinary exposure-safe painting, never XOR.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native.h>
#include <xview/panel.h>
#include <xview/cms.h>
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

    void repaint_grip(Panel panel, Xv_Window window, Rectlist *) {
        auto *owner = reinterpret_cast<native::split_view *>(
            xv_get(panel, WIN_CLIENT_DATA));
        if (!owner || !owner->get_created() || !window)
            return;
        const auto bar = owner->get_splitter_bounds();
        const bool vertical = owner->get_orientation() ==
            native::split_orientation::horizontal;
        const int width = vertical ? bar.d.w - 2 : std::min<int>(18, bar.d.w);
        const int height = vertical ? std::min<int>(18, bar.d.h) : bar.d.h - 2;
        if (width < 3 || height < 3)
            return;
        const int x = bar.p.x + (bar.d.w - width) / 2;
        const int y = bar.p.y + (bar.d.h - height) / 2;
        Display *display = linux::openlook::cached_display;
        const auto drawable = static_cast<Window>(xv_get(window, XV_XID));
        const auto cms = xv_get(panel, WIN_CMS);
        GC gc = XCreateGC(display, drawable, 0, nullptr);
        // Short raised ribs identify the movable strip without another
        // boxed control beside the native scrollbar. Rotate with the split.
        const int length = vertical ? height : width;
        for (int offset = 0; offset + 1 < length; offset += 4) {
            XSetForeground(display, gc,
                WhitePixel(display, DefaultScreen(display)));
            if (vertical)
                XDrawLine(display, drawable, gc,
                    x, y + offset, x + width - 1, y + offset);
            else
                XDrawLine(display, drawable, gc,
                    x + offset, y, x + offset, y + height - 1);
            XSetForeground(display, gc, xv_get(cms, CMS_FOREGROUND_PIXEL));
            if (vertical)
                XDrawLine(display, drawable, gc,
                    x, y + offset + 1, x + width - 1, y + offset + 1);
            else
                XDrawLine(display, drawable, gc,
                    x + offset + 1, y, x + offset + 1, y + height - 1);
        }
        XFreeGC(display, gc);
    }

    Notify_value handle_splitter_event(
        Panel panel,
        Event *event,
        Notify_arg argument,
        Notify_event_type type) {
        auto *owner = reinterpret_cast<native::split_view *>(
            xv_get(panel, WIN_CLIENT_DATA));
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !event ||
            !linux::openlook::permit_input(owner)) {
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
            XGrabPointer(linux::openlook::cached_display,
                static_cast<Window>(xv_get(panel, XV_XID)), False,
                ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                position));
            return NOTIFY_DONE;
        } else if (action == LOC_DRAG && state->dragging) {
            owner->on_native_mouse_move(position);
            return NOTIFY_DONE;
        } else if (action == ACTION_SELECT &&
                   !event_is_down(event) && state->dragging) {
            state->dragging = false;
            XUngrabPointer(linux::openlook::cached_display, CurrentTime);
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                position));
            return NOTIFY_DONE;
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
            PANEL_REPAINT_PROC, repaint_grip,
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
            if (state->first) xv_destroy(state->first);
            if (state->second) xv_destroy(state->second);
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
        destroy_children();
        if (state && state->host) {
            if (state->dragging)
                XUngrabPointer(linux::openlook::cached_display, CurrentTime);
            linux::openlook::wnd_bindings.unregister_by_handle(state->host);
            const Panel host = state->host;
            const Panel first = state->first;
            const Panel second = state->second;
            xv_set(host, WIN_CLIENT_DATA, nullptr, nullptr);
            app::post([host, first, second] {
                // A Panel destroys its items, not nested Panels. Release
                // both pane objects before X destroys their parent window.
                xv_destroy(first);
                xv_destroy(second);
                xv_destroy(host);
            });
        }
        linux::openlook::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
