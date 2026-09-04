//
// Implements the GEMix window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/wnd.h>

#include "globals.h"
#include "gpx_wnd.h"

namespace
{
    // Apply cached bounds to an AES window. Public geometry names the
    // client area, but AES is set through the outer rectangle, so the
    // size grows by the window's own decorations on the way out.
    void apply_window_bounds(native::wnd *window,
                             const native::rect &bounds) {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(window);
        if (handle <= 0)
            return;

        const native::size outer =
            linux::gemix::outer_size_for(handle, bounds.d);
        wind_set(handle,
                 WF_CURRXYWH,
                 bounds.p.x,
                 bounds.p.y,
                 outer.w,
                 outer.h);
    }
} // namespace

namespace native
{
    void wnd::apply_position() {
        apply_window_bounds(this, _bounds);
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_dimensions() {
        apply_window_bounds(this, _bounds);
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_bounds() {
        apply_window_bounds(this, _bounds);
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_parent() {
        if (_parent) {
            _parent->invalidate();
        }
    }

    void wnd::apply_cursor() {
        if (!linux::gemix::runtime.initialized)
            return;

        WORD x = 0;
        WORD y = 0;
        WORD buttons = 0;
        WORD modifiers = 0;
        graf_mkstate(&x, &y, &buttons, &modifiers);
        const WORD handle = wind_find(x, y);
        auto *root = dynamic_cast<app_wnd *>(
            linux::gemix::wnd_bindings.object_from_handle(handle));
        if (!root || linux::gemix::root_of(this) != root)
            return;

        const rect work = linux::gemix::work_rect(handle);
        linux::gemix::update_mouse_cursor(
            root, point(x - work.p.x, y - work.p.y));
    }

    wnd &wnd::invalidate_native() {
        if (auto *p = get_parent())
            p->invalidate();
        else
            linux::gemix::request_repaint(this);
        return *this;
    }

    wnd &wnd::invalidate_native(const rect &area) {
        if (auto *parent = get_parent())
            parent->invalidate(area);
        else
            linux::gemix::request_repaint(
                this, &area);
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
