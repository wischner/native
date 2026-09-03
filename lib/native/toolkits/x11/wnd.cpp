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

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"
#include "window_position.h"

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
        } else if (widget && !dynamic_cast<split_view *>(_parent)) {
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
        } else if (widget && !dynamic_cast<split_view *>(_parent)) {
            XtVaSetValues(widget,
                          XtNwidth,
                          linux::x11::widget_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::widget_dimension(_bounds.d.h),
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
        } else if (widget && !dynamic_cast<split_view *>(_parent)) {
            XtVaSetValues(widget,
                          XtNhorizDistance,
                          _bounds.p.x,
                          XtNvertDistance,
                          _bounds.p.y,
                          XtNwidth,
                          linux::x11::widget_dimension(_bounds.d.w),
                          XtNheight,
                          linux::x11::widget_dimension(_bounds.d.h),
                          nullptr);
        }
    }

    void wnd::apply_parent() {
        if (dynamic_cast<app_wnd *>(this))
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

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
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

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
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

        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }
} // namespace native
