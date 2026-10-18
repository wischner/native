//
// Implements X11 window geometry and invalidation through Xt widgets.
// Application shells and Athena child controls remain private bindings.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/cursorfont.h>

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"
#include "scale.h"
#include "../x_pointer.h"
#include "../x_border.h"
#include "window_position.h"

namespace
{
    struct pointer_tracking { bool installed = false; };
    void track_pointer(Widget, XtPointer data, XEvent *event, Boolean *) {
        auto *owner = static_cast<native::wnd *>(data);
        const auto weak = native::detail::mouse_access::lifetime(*owner);
        if (event->type == LeaveNotify) {
            native::detail::pointer_leave(*owner, linux::x11::pointer_to_logical(
                event->xcrossing.x, event->xcrossing.y));
        } else if (event->type == EnterNotify || event->type == MotionNotify) {
            native::point local = event->type == MotionNotify
                ? linux::x11::pointer_to_logical(event->xmotion.x, event->xmotion.y)
                : linux::x11::pointer_to_logical(event->xcrossing.x, event->xcrossing.y);
            native::detail::pointer_hover(owner, local);
        }
        auto lifetime = weak.lock();
        if (lifetime && lifetime->alive) native::detail::mouse_access::refresh(*owner);
    }
    Dimension backing_dimension(Widget widget, int value) {
        Dimension border = 0;
        XtVaGetValues(widget, XtNborderWidth, &border, nullptr);
        return linux::x11::widget_dimension(
            linux::x11::to_device(value) - 2 * border);
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
        Widget widget = linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget || !XtIsRealized(widget) ||
            linux::x11::shell_bindings.handle_from_object(this)) return;
        Dimension border = 0;
        XtVaGetValues(widget, XtNborderWidth, &border, nullptr);
        const int inside = dynamic_cast<combo_box *>(this) ? 1 : 0;
        if (!border && !inside) return;
        detail::shape_border(XtDisplay(widget), XtWindow(widget),
                             get_border_sides(), inside, border);
    }

    void wnd::apply_position() {
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);

        if (shell) {
            const point position =
                linux::x11::constrain_shell_position(
                    shell, _bounds.p, _bounds.d);
            XtVaSetValues(shell,
                          XtNx,
                          linux::x11::to_device(position.x),
                          XtNy,
                          linux::x11::to_device(position.y),
                          nullptr);
        } else if (widget) {
            XtVaSetValues(widget,
                          XtNhorizDistance,
                          linux::x11::to_device(_bounds.p.x),
                          XtNvertDistance,
                          linux::x11::to_device(_bounds.p.y),
                          nullptr);
        }
    }

    void wnd::apply_dimensions() {
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(shell,
                          XtNwidth,
                          linux::x11::device_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::device_dimension(_bounds.d.h),
                          nullptr);
            const point position =
                linux::x11::constrain_shell_position(
                    shell, _bounds.p, _bounds.d);
            XtVaSetValues(shell,
                          XtNx,
                          linux::x11::to_device(position.x),
                          XtNy,
                          linux::x11::to_device(position.y),
                          nullptr);
        } else if (widget) {
            XtVaSetValues(widget,
                          XtNwidth,
                          backing_dimension(widget, _bounds.d.w),
                          XtNheight,
                          backing_dimension(widget, _bounds.d.h),
                          nullptr);
        }
    }

    void wnd::apply_bounds() {
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);

        if (shell) {
            const point position =
                linux::x11::constrain_shell_position(
                    shell, _bounds.p, _bounds.d);
            XtVaSetValues(shell,
                          XtNx,
                          linux::x11::to_device(position.x),
                          XtNy,
                          linux::x11::to_device(position.y),
                          XtNwidth,
                          linux::x11::device_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::device_dimension(_bounds.d.h),
                          nullptr);
        } else if (widget) {
            XtVaSetValues(widget,
                          XtNhorizDistance,
                          linux::x11::to_device(_bounds.p.x),
                          XtNvertDistance,
                          linux::x11::to_device(_bounds.p.y),
                          XtNwidth,
                          backing_dimension(widget, _bounds.d.w),
                          XtNheight,
                          backing_dimension(widget, _bounds.d.h),
                          nullptr);
        }
    }

    void wnd::apply_parent() {
        if (linux::x11::shell_bindings.handle_from_object(this))
            return;

        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        const bool was_visible = widget && XtIsManaged(widget);

        destroy();
        if (_parent) {
            create();
            if (was_visible)
                show();
        }
    }

    void wnd::apply_cursor() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget || !XtIsRealized(widget))
            return;

        Display *display = XtDisplay(widget);
        auto *tracking = detail::peer_state<pointer_tracking>(*this);
        if (!tracking) {
            tracking = new pointer_tracking;
            detail::assign_peer_state(*this, tracking);
        }
        if (!tracking->installed) {
            XtAddEventHandler(widget, EnterWindowMask | LeaveWindowMask | PointerMotionMask,
                False, track_pointer, this);
            tracking->installed = true;
        }
        const point device =
            detail::x_pointer_position(display, XtWindow(widget));
        Cursor cursor = cursor_for(display, detail::pointer_cursor(*this,
            linux::x11::pointer_to_logical(device.x, device.y)));
        if (cursor != None) {
            XDefineCursor(display, XtWindow(widget), cursor);
            XFreeCursor(display, cursor);
        }
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;

        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            this);
        if (widget && XtIsRealized(widget)) {
            XClearArea(linux::x11::cached_display,
                       XtWindow(widget),
                       0,
                       0,
                       0,
                       0,
                       True);
            XFlush(linux::x11::cached_display);
        }

        return *this;
    }

    wnd &wnd::invalidate_native(const rect &r) {
        if (!_created)
            return *this;

        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            this);
        if (widget && XtIsRealized(widget)) {
            const rect device = linux::x11::rect_to_device(r);
            XClearArea(linux::x11::cached_display,
                       XtWindow(widget),
                       device.p.x,
                       device.p.y,
                       device.d.w,
                       device.d.h,
                       True);
            XFlush(linux::x11::cached_display);
        }

        return *this;
    }

    gpx &wnd::get_gpx() {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

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
        Widget widget = linux::x11::wnd_bindings.handle_from_object(&owner);
        Display *display = widget ? XtDisplay(widget) : nullptr;
        Window target = widget && XtIsRealized(widget) ? XtWindow(widget) : None;
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
        Widget widget = linux::x11::wnd_bindings.handle_from_object(&owner);
        Display *display = widget ? XtDisplay(widget) : nullptr;
        Window target = widget && XtIsRealized(widget) ? XtWindow(widget) : None;
        (void)target;
        if (display) XUngrabPointer(display, CurrentTime);
        backend_refresh_mouse(owner);
    }
    void backend_refresh_mouse(wnd &owner) {
        mouse_access::refresh(owner);
    }
}
