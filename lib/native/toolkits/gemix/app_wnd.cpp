//
// Implements the GEMix application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <native.h>

#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(this);
        if (handle <= 0)
            throw std::runtime_error(
                "GEMix: Missing window binding for app_wnd.");

        wind_set_str(handle, WF_NAME, _title.c_str());
    }

    void app_wnd::create() const {
        if (_created)
            return;

        if (!linux::gemix::ensure_runtime())
            throw std::runtime_error("GEMix: failed to initialize AES/VDI runtime.");

        rect desktop = linux::gemix::desktop_rect();
        WORD handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
                                  desktop.p.x, desktop.p.y, desktop.d.w, desktop.d.h);
        if (handle < 0)
            throw std::runtime_error("GEMix: failed to create window.");

        linux::gemix::wnd_bindings.register_pair(handle, const_cast<app_wnd *>(this));
        wind_set_str(handle, WF_NAME, _title.c_str());
        _created = true;

        const_cast<app_wnd *>(this)->menu.attach(
            *const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "GEMix: Cannot show window before it is created.");

        WORD handle = linux::gemix::wnd_bindings.handle_from_object(const_cast<app_wnd *>(this));
        if (handle <= 0)
            throw std::runtime_error(
                "GEMix: Missing window binding for app_wnd.");

        wind_open(handle, _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h);
        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;
        wind_get(handle, WF_CURRXYWH, &x, &y, &w, &h);
        const_cast<app_wnd *>(this)->_bounds = rect(x, y, w, h);
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        WORD handle = linux::gemix::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (handle > 0) {
            wind_close(handle);
            wind_delete(handle);
            linux::gemix::wnd_bindings.unregister_by_handle(handle);
        }
        if (app::main_wnd() == this)
            linux::gemix::runtime.shutdown_requested = true;
    }
}
