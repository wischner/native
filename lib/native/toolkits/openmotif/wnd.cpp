//
// Implements the OpenMotif window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/XmP.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"
#include "../x_border.h"

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
    void wnd::apply_border_sides() {
        Widget widget = linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!widget || !XtIsRealized(widget) ||
            linux::openmotif::shell_bindings.handle_from_object(this)) return;
        Dimension border = 0, shadow = 0, highlight = 0;
        XtVaGetValues(widget, XtNborderWidth, &border, nullptr);
        if (XmIsPrimitive(widget) || XmIsManager(widget))
            XtVaGetValues(widget, XmNshadowThickness, &shadow, nullptr);
        if (!border && !shadow) return;
        if (XmIsPrimitive(widget))
            XtVaGetValues(widget, XmNhighlightThickness, &highlight, nullptr);
        // Primitive relief starts inside the keyboard-focus reservation.
        int inside = shadow + highlight;
        if (XmIsPushButton(widget)) {
            Dimension default_shadow = 0;
            XtVaGetValues(widget, XmNdefaultButtonShadowThickness, &default_shadow, nullptr);
            // Motif adds a default-button ring, a shadow-width gap and
            // enhancement pixels hidden by its highlight resource getter.
            if (default_shadow)
                inside += 2 * default_shadow + shadow + Xm3D_ENHANCE_PIXEL;
        }
        detail::shape_border(XtDisplay(widget), XtWindow(widget),
                             get_border_sides(), inside, border);
    }

    void wnd::apply_position() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(
                shell, XtNx, _bounds.p.x, XtNy, _bounds.p.y, nullptr);
        } else if (widget) {
            XtVaSetValues(
                widget, XmNx, _bounds.p.x, XmNy, _bounds.p.y, nullptr);
        }
    }

    void wnd::apply_dimensions() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(shell,
                          XtNwidth,
                          _bounds.d.w,
                          XtNheight,
                          _bounds.d.h,
                          nullptr);
        }
        if (widget) {
            XtVaSetValues(widget,
                          XmNwidth,
                          _bounds.d.w,
                          XmNheight,
                          _bounds.d.h,
                          nullptr);
        }
    }

    void wnd::apply_bounds() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(shell,
                          XtNx,
                          _bounds.p.x,
                          XtNy,
                          _bounds.p.y,
                          XtNwidth,
                          _bounds.d.w,
                          XtNheight,
                          _bounds.d.h,
                          nullptr);
        }
        if (widget) {
            XtVaSetValues(widget,
                          XmNx,
                          shell ? 0 : _bounds.p.x,
                          XmNy,
                          shell ? 0 : _bounds.p.y,
                          XmNwidth,
                          _bounds.d.w,
                          XmNheight,
                          _bounds.d.h,
                          nullptr);
        }
    }

    void wnd::apply_parent() {
        if (linux::openmotif::shell_bindings.handle_from_object(this))
            return;

        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);
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
            linux::openmotif::wnd_bindings.handle_from_object(this);
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

        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(
                this);
        if (canvas && XtIsRealized(canvas)) {
            XClearArea(linux::openmotif::cached_display,
                       XtWindow(canvas),
                       0,
                       0,
                       0,
                       0,
                       True);
            XFlush(linux::openmotif::cached_display);
        }

        return *this;
    }

    wnd &wnd::invalidate_native(const rect &r) {
        if (!_created)
            return *this;

        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(
                this);
        if (canvas && XtIsRealized(canvas)) {
            XClearArea(linux::openmotif::cached_display,
                       XtWindow(canvas),
                       r.p.x,
                       r.p.y,
                       r.d.w,
                       r.d.h,
                       True);
            XFlush(linux::openmotif::cached_display);
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
