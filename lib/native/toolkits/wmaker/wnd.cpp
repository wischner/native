//
// Implements portable geometry, parenting, invalidation, and graphics
// access for WINGs windows and controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../property_grid_peer.h"
#include <algorithm>
#include <stdexcept>

#include <X11/cursorfont.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/app_wnd.h>
#include <native/combo_box.h>
#include <native/tab_view.h>
#include <native/wnd.h>
#include <native/button.h>
#include <native/accordion.h>
#include <native/icon_view.h>
#include <native/text_edit.h>
#include <native/list.h>
#include <native/tree_view.h>
#include <native/table_view.h>

#include "../../gpx_wnd.h"
#include "globals.h"
#include "collection_host.h"
#include "../x_pointer.h"
#include "../x_border.h"

namespace
{
    struct pointer_tracking
    {
        Display *display = nullptr;
        Cursor cursor = None;
        native::mouse_cursor shape = native::mouse_cursor::arrow;
        ~pointer_tracking() {
            if (display && cursor)
                XFreeCursor(display, cursor);
        }
    };
    native::point pointer_local(native::wnd &owner, native::point point) {
        if (auto *state = native::detail::peer_state<linux::wmaker::window_state>(owner))
            point.y = static_cast<native::coord>(point.y - state->menu_height);
        return point;
    }
    void track_pointer(XEvent *event, void *data) {
        auto *owner = static_cast<native::wnd *>(data);
        const auto weak = native::detail::mouse_access::lifetime(*owner);
        const XEvent copy = *event;
        linux::wmaker::defer([owner, weak, copy] {
            auto lifetime = weak.lock();
            if (!lifetime || !lifetime->alive) return;
            if (copy.type == LeaveNotify)
                native::detail::pointer_leave(*owner, pointer_local(*owner,
                    native::point(copy.xcrossing.x, copy.xcrossing.y)));
            else if (copy.type == EnterNotify || copy.type == MotionNotify) {
                auto point = copy.type == MotionNotify
                    ? native::point(copy.xmotion.x, copy.xmotion.y)
                    : native::point(copy.xcrossing.x, copy.xcrossing.y);
                native::detail::pointer_hover(owner, pointer_local(*owner, point));
            }
            if (lifetime->alive) native::detail::mouse_access::refresh(*owner);
        });
    }
    Cursor cursor_for(Display *display, native::mouse_cursor cursor) {
        if (!display)
            return None;

        if (cursor == native::mouse_cursor::hidden)
            return native::detail::invisible_x_cursor(
                display, DefaultRootWindow(display));
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
    void wnd::apply_border_sides() {
        if (auto *editor = dynamic_cast<text_edit *>(this)) {
            auto *binding = linux::wmaker::text_edit_bindings.object_from_handle(editor);
            if (binding && binding->text && native::detail::property_grid_owner(*this))
                return;
        }
        if (!(dynamic_cast<button *>(this) || dynamic_cast<text_edit *>(this) ||
              dynamic_cast<combo_box *>(this) || dynamic_cast<list *>(this) ||
              dynamic_cast<tree_view *>(this) || dynamic_cast<table_view *>(this) ||
              dynamic_cast<tab_view *>(this) || dynamic_cast<icon_view *>(this) ||
              dynamic_cast<accordion *>(this))) return;
        WMWidget *widget = linux::wmaker::wnd_bindings.handle_from_object(this);
        if (widget) detail::shape_border(linux::wmaker::display,
            WMWidgetXID(widget), get_border_sides(), 2);
    }

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
        if (auto *editor = dynamic_cast<text_edit *>(this))
            if (auto *state = linux::wmaker::text_edit_bindings.object_from_handle(editor))
                linux::wmaker::configure_text_cell(*editor, *state);
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
        Display *display = linux::wmaker::display;
        auto *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (!display || !widget)
            return;

        auto *tracking = detail::peer_state<pointer_tracking>(*this);
        if (!tracking) {
            tracking = new pointer_tracking;
            tracking->display = display;
            detail::assign_peer_state(*this, tracking);
            WMCreateEventHandler(WMWidgetView(widget),
                EnterWindowMask | LeaveWindowMask | PointerMotionMask, track_pointer, this);
        }
        const auto target = WMWidgetXID(widget);
        // Native editors consume their own crossing events. Their
        // normal I-beam belongs to the whole view, including when the
        // pointer was elsewhere at creation; do not initialize that
        // view with an arrow.
        const bool native_text = dynamic_cast<text_edit *>(this) &&
                                 get_cursor() == mouse_cursor::ibeam;
        const auto shape =
            native_text ? mouse_cursor::ibeam
            : target    ? detail::pointer_cursor(
                              *this,
                              pointer_local(*this,
                                            detail::x_pointer_position(
                                                display, target)))
                        : get_cursor();
        const bool changed =
            !tracking->cursor || tracking->shape != shape;
        const Cursor cursor =
            changed ? cursor_for(display, shape) : tracking->cursor;
        if (cursor == None)
            return;
        const auto previous = changed ? tracking->cursor : None;
        tracking->cursor = cursor;
        tracking->shape = shape;
        W_SetViewCursor(WMWidgetView(widget), cursor);
        if (auto *text = dynamic_cast<text_edit *>(this))
            if (auto *state = linux::wmaker::text_edit_bindings
                                  .object_from_handle(text);
                state && state->text)
                W_SetViewCursor(WMWidgetView(state->text), cursor);
        if (previous)
            XFreeCursor(display, previous);
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;
        auto *self = this;
        if (linux::wmaker::schedule_collection_repaint(*self))
            return *this;
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
        if (linux::wmaker::schedule_collection_repaint(*self))
            return *this;
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

namespace native::detail
{
    mouse_capabilities backend_mouse_capabilities() {
        return {true, true, false, false};
    }
    bool backend_capture_mouse(wnd &owner, mouse_capture_options,
                               std::string &error) {
        Display *display = linux::wmaker::display;
        Window target = linux::wmaker::drawable(&owner);
        if (!display || target == None) {
            error = "Native pointer surface is unavailable.";
            return false;
        }
        const int result = XGrabPointer(display, target, False,
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        if (result != GrabSuccess) {
            error = "X11 pointer grab failed or another grab is active.";
            return false;
        }
        return true;
    }
    void backend_release_mouse(wnd &owner, mouse_capture_mode) {
        Display *display = linux::wmaker::display;
        Window target = linux::wmaker::drawable(&owner);
        (void)target;
        if (display) XUngrabPointer(display, CurrentTime);
        backend_refresh_mouse(owner);
    }
    void backend_refresh_mouse(wnd &owner) {
        mouse_access::refresh(owner);
    }
}
