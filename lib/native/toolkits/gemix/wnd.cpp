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

namespace native
{
    void wnd::apply_position() {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(this);
        if (handle > 0) {
            wind_set(handle,
                     WF_CURRXYWH,
                     _bounds.p.x,
                     _bounds.p.y,
                     _bounds.d.w,
                     _bounds.d.h);
        }
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_dimensions() {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(this);
        if (handle > 0) {
            wind_set(handle,
                     WF_CURRXYWH,
                     _bounds.p.x,
                     _bounds.p.y,
                     _bounds.d.w,
                     _bounds.d.h);
        }
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_bounds() {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(this);
        if (handle > 0) {
            wind_set(handle,
                     WF_CURRXYWH,
                     _bounds.p.x,
                     _bounds.p.y,
                     _bounds.d.w,
                     _bounds.d.h);
        }
        if (_parent)
            _parent->invalidate();
    }

    void wnd::apply_parent() {
        if (_parent) {
            _parent->invalidate();
        }
    }

    wnd &wnd::invalidate() const {
        if (auto *p = get_parent())
            p->invalidate();
        else
            linux::gemix::request_repaint(const_cast<wnd *>(this));
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &) const {
        return invalidate();
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
