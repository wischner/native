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
#include "window_position.h"

namespace
{
    Dimension backing_dimension(Widget widget, int value) {
        Dimension border = 0;
        XtVaGetValues(widget, XtNborderWidth, &border, nullptr);
        return linux::x11::widget_dimension(value - 2 * border);
    }
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
                          position.x,
                          XtNy,
                          position.y,
                          nullptr);
        } else if (widget) {
            XtVaSetValues(widget,
                          XtNhorizDistance,
                          _bounds.p.x,
                          XtNvertDistance,
                          _bounds.p.y,
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
                          linux::x11::widget_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::widget_dimension(_bounds.d.h),
                          nullptr);
            const point position =
                linux::x11::constrain_shell_position(
                    shell, _bounds.p, _bounds.d);
            XtVaSetValues(shell,
                          XtNx,
                          position.x,
                          XtNy,
                          position.y,
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
                          position.x,
                          XtNy,
                          position.y,
                          XtNwidth,
                          linux::x11::widget_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::widget_dimension(_bounds.d.h),
                          nullptr);
        } else if (widget) {
            XtVaSetValues(widget,
                          XtNhorizDistance,
                          _bounds.p.x,
                          XtNvertDistance,
                          _bounds.p.y,
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
        Cursor cursor = cursor_for(display, _cursor);
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
            XClearArea(linux::x11::cached_display,
                       XtWindow(widget),
                       r.p.x,
                       r.p.y,
                       r.d.w,
                       r.d.h,
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
