//
// Implements the OpenMotif window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(
                shell,
                XtNx, _bounds.p.x,
                XtNy, _bounds.p.y,
                nullptr);
        }
        else if (widget) {
            XtVaSetValues(
                widget,
                XmNx, _bounds.p.x,
                XmNy, _bounds.p.y,
                nullptr);
        }
    }

    void wnd::apply_dimensions() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(
                shell,
                XtNwidth, _bounds.d.w,
                XtNheight, _bounds.d.h,
                nullptr);
        }
        if (widget) {
            XtVaSetValues(
                widget,
                XmNwidth, _bounds.d.w,
                XmNheight, _bounds.d.h,
                nullptr);
        }
    }

    void wnd::apply_bounds() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);

        if (shell) {
            XtVaSetValues(
                shell,
                XtNx, _bounds.p.x,
                XtNy, _bounds.p.y,
                XtNwidth, _bounds.d.w,
                XtNheight, _bounds.d.h,
                nullptr);
        }
        if (widget) {
            XtVaSetValues(
                widget,
                XmNx, shell ? 0 : _bounds.p.x,
                XmNy, shell ? 0 : _bounds.p.y,
                XmNwidth, _bounds.d.w,
                XmNheight, _bounds.d.h,
                nullptr);
        }
    }

    void wnd::apply_parent() {
        auto *control = dynamic_cast<button *>(this);
        if (!control)
            return;

        auto *binding =
            linux::openmotif::button_bindings.object_from_handle(control);
        const bool was_visible =
            binding && binding->widget && XtIsManaged(binding->widget);

        control->destroy();
        if (_parent) {
            control->create();
            if (was_visible)
                control->show();
        }
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget canvas = linux::openmotif::wnd_bindings.handle_from_object(const_cast<wnd *>(this));
        if (canvas && XtIsRealized(canvas)) {
            XClearArea(linux::openmotif::cached_display, XtWindow(canvas), 0, 0, 0, 0, True);
            XFlush(linux::openmotif::cached_display);
        }

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget canvas = linux::openmotif::wnd_bindings.handle_from_object(const_cast<wnd *>(this));
        if (canvas && XtIsRealized(canvas)) {
            XClearArea(
                linux::openmotif::cached_display,
                XtWindow(canvas),
                r.p.x,
                r.p.y,
                r.d.w,
                r.d.h,
                True);
            XFlush(linux::openmotif::cached_display);
        }

        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error("Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
