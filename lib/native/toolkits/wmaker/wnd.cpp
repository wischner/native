//
// Implements portable geometry, parenting, invalidation, and graphics
// access for WINGs windows and controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/cursorfont.h>
#include <WINGs/WINGs.h>

#include <native/app_wnd.h>
#include <native/combo_box.h>
#include <native/tab_view.h>
#include <native/wnd.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace
{
    Cursor cursor_for(Display *display, native::mouse_cursor cursor) {
        if (!display)
            return None;

        unsigned int shape = XC_left_ptr;
        if (cursor == native::mouse_cursor::ibeam)
            shape = XC_xterm;
        else if (cursor == native::mouse_cursor::crosshair)
            shape = XC_crosshair;
        else if (cursor == native::mouse_cursor::resize_horizontal)
            shape = XC_sb_h_double_arrow;
        else if (cursor == native::mouse_cursor::resize_vertical)
            shape = XC_sb_v_double_arrow;
        else if (cursor == native::mouse_cursor::resize_northwest_southeast)
            shape = XC_bottom_right_corner;
        else if (cursor == native::mouse_cursor::resize_northeast_southwest)
            shape = XC_bottom_left_corner;
        return XCreateFontCursor(display, shape);
    }
} // namespace

namespace native
{
    void wnd::apply_position() {
        if (auto *window_state = native::detail::peer_state<
                linux::wmaker::window_state>(*this)) {
            if (!window_state || !window_state->window)
                return;
            const point position =
                linux::wmaker::constrain_position(_bounds.p, _bounds.d);
            WMSetWindowUserPosition(
                window_state->window, position.x, position.y);
            if (WMWidgetXID(window_state->window) != None) {
                XMoveWindow(linux::wmaker::display,
                            WMWidgetXID(window_state->window),
                            position.x,
                            position.y);
            }
            return;
        }

        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (widget) {
            const point position =
                linux::wmaker::control_position(this);
            WMMoveWidget(widget, position.x, position.y);
        }
    }

    void wnd::apply_dimensions() {
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (!widget)
            return;
        int height = _bounds.d.h;
        if (auto *window_state = native::detail::peer_state<
                linux::wmaker::window_state>(*this)) {
            height += window_state ? window_state->menu_height : 0;
        }
        WMResizeWidget(widget,
                       static_cast<unsigned int>(std::max(
                           1, static_cast<int>(_bounds.d.w))),
                       static_cast<unsigned int>(std::max(1, height)));
        if (auto *state = native::detail::peer_state<
                linux::wmaker::native_tab_view>(*this)) {
            if (state && state->portable) {
                auto *tabs = static_cast<tab_view *>(this);
                const rect content = tabs->get_content_bounds();
                for (WMFrame *page : state->pages) {
                    WMMoveWidget(page, content.p.x, content.p.y);
                    WMResizeWidget(
                        page,
                        static_cast<unsigned int>(std::max(
                            1, static_cast<int>(content.d.w))),
                        static_cast<unsigned int>(std::max(
                            1, static_cast<int>(content.d.h))));
                }
            }
        }
        if (auto *state = native::detail::peer_state<
                linux::wmaker::native_combo_box>(*this)) {
            if (!state || !state->field || !state->popup)
                return;
            auto *combo = static_cast<combo_box *>(this);
            linux::wmaker::configure_combo_box(*combo, *state);
        }
    }

    void wnd::apply_bounds() {
        apply_dimensions();
        apply_position();
    }

    void wnd::apply_parent() {
        if (native::detail::peer_state<
                linux::wmaker::window_state>(*this))
            return;
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (!widget)
            return;
        const point position =
            linux::wmaker::control_position(this);
        WMReparentWidget(widget,
                         linux::wmaker::parent_widget(this),
                         position.x,
                         position.y);
    }

    void wnd::apply_cursor() {
        const Window target = linux::wmaker::drawable(this);
        Display *display = linux::wmaker::display;
        if (!display || target == None)
            return;

        Cursor cursor = cursor_for(display, _cursor);
        if (cursor != None) {
            XDefineCursor(display, target, cursor);
            XFreeCursor(display, cursor);
        }
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;
        auto *self = this;
        if (native::detail::peer_state<
                linux::wmaker::window_state>(*self)) {
            auto *window = static_cast<app_wnd *>(self);
            linux::wmaker::schedule_repaint(
                window,
                rect({0, 0}, window->get_dimensions()));
        } else if (WMWidget *widget =
                       linux::wmaker::wnd_bindings
                           .handle_from_object(self)) {
            WMRedisplayWidget(widget);
        }
        return *self;
    }

    wnd &wnd::invalidate_native(const rect &area) {
        if (!_created)
            return *this;
        auto *self = this;
        if (native::detail::peer_state<
                linux::wmaker::window_state>(*self)) {
            auto *window = static_cast<app_wnd *>(self);
            linux::wmaker::schedule_repaint(window, area);
        } else if (WMWidget *widget =
                       linux::wmaker::wnd_bindings
                           .handle_from_object(self)) {
            WMRedisplayWidget(widget);
        }
        return *self;
    }

    gpx &wnd::get_gpx() {
        if (!_created) {
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");
        }
        if (!_gpx)
            _gpx = new gpx_wnd(this);
        return *_gpx;
    }
} // namespace native
