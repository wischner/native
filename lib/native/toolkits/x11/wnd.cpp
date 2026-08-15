//
// Implements the X11 window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xlib.h>

#include <native.h>
#include "bindings.h"
#include "gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        Window window = linux::x11::wnd_bindings.handle_from_object(this);
        if (linux::x11::cached_display && window) {
            XMoveWindow(
                linux::x11::cached_display,
                window,
                _bounds.p.x,
                _bounds.p.y);
            XFlush(linux::x11::cached_display);
        }
    }

    void wnd::apply_dimensions() {
        Window window = linux::x11::wnd_bindings.handle_from_object(this);
        if (linux::x11::cached_display && window) {
            XResizeWindow(
                linux::x11::cached_display,
                window,
                _bounds.d.w,
                _bounds.d.h);
            XFlush(linux::x11::cached_display);
        }
    }

    void wnd::apply_bounds() {
        Window window = linux::x11::wnd_bindings.handle_from_object(this);
        if (linux::x11::cached_display && window) {
            XMoveResizeWindow(
                linux::x11::cached_display,
                window,
                _bounds.p.x,
                _bounds.p.y,
                _bounds.d.w,
                _bounds.d.h);
            XFlush(linux::x11::cached_display);
        }
    }

    void wnd::apply_parent() {
        if (!linux::x11::cached_display)
            return;

        Window child = linux::x11::wnd_bindings.handle_from_object(this);
        Window parent = _parent
                            ? linux::x11::wnd_bindings.handle_from_object(
                                  _parent)
                            : DefaultRootWindow(
                                  linux::x11::cached_display);
        if (child && parent) {
            XReparentWindow(
                linux::x11::cached_display,
                child,
                parent,
                _bounds.p.x,
                _bounds.p.y);
            XFlush(linux::x11::cached_display);
        }
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Window win = linux::x11::wnd_bindings.handle_from_object(const_cast<wnd *>(this));
        XClearArea(linux::x11::cached_display, win, 0, 0, 0, 0, True);
        XFlush(linux::x11::cached_display);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        Window win = linux::x11::wnd_bindings.handle_from_object(const_cast<wnd *>(this));
        XClearArea(linux::x11::cached_display, win, r.p.x, r.p.y, r.d.w, r.d.h, True);
        XFlush(linux::x11::cached_display);
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
