#include <stdexcept>
#include <utility>

#include <native.h>

#include "globals.h"

namespace native
{
    app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _title(std::move(title))
    {
    }

    app_wnd::app_wnd(const std::string &title, const point &pos, const size &dim)
        : app_wnd(title, pos.x, pos.y, dim.w, dim.h)
    {
    }

    app_wnd::app_wnd(const std::string &title, const rect &bounds)
        : app_wnd(title, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;
        if (_created)
        {
            WORD handle = gemix::wnd_bindings.from_b(const_cast<app_wnd *>(this));
            if (handle > 0)
                wind_set_str(handle, WF_NAME, _title.c_str());
        }
        return *this;
    }

    void app_wnd::create() const
    {
        if (_created)
            return;

        if (!gemix::ensure_runtime())
            throw std::runtime_error("GEMix: failed to initialize AES/VDI runtime.");

        rect desktop = gemix::desktop_rect();
        WORD handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
                                  desktop.p.x, desktop.p.y, desktop.d.w, desktop.d.h);
        if (handle < 0)
            throw std::runtime_error("GEMix: failed to create window.");

        gemix::wnd_bindings.register_pair(handle, const_cast<app_wnd *>(this));
        wind_set_str(handle, WF_NAME, _title.c_str());
        _created = true;

        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        WORD handle = gemix::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (handle > 0)
        {
            wind_open(handle, _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h);
            WORD x = 0;
            WORD y = 0;
            WORD w = 0;
            WORD h = 0;
            wind_get(handle, WF_CURRXYWH, &x, &y, &w, &h);
            const_cast<app_wnd *>(this)->_bounds = rect(x, y, w, h);
        }
        const_cast<app_wnd *>(this)->menu.attach(*const_cast<app_wnd *>(this));
        invalidate();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        gemix::destroy_menu(const_cast<app_wnd *>(this));

        WORD handle = gemix::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (handle > 0)
        {
            wind_close(handle);
            wind_delete(handle);
            gemix::wnd_bindings.unregister_by_a(handle);
        }

        _created = false;
        if (app::main_wnd() == this)
            gemix::runtime.shutdown_requested = true;
    }
}
