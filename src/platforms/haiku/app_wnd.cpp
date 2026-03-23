#include <stdexcept>
#include <utility>

#include <Application.h>
#include <Window.h>

#include <native.h>

#include "NativeWindow.h"
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

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;

        if (_created)
        {
            BWindow *win = haiku::wnd_bindings.from_b(this);
            if (win && win->Lock())
            {
                win->SetTitle(_title.c_str());
                win->Unlock();
            }
        }

        return *this;
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    void app_wnd::create() const
    {
        if (_created)
            return;

        if (!haiku::global_app)
            haiku::global_app = new BApplication("application/x-vnd.wischner-native");

        BRect frame(
            static_cast<float>(_bounds.p.x),
            static_cast<float>(_bounds.p.y),
            static_cast<float>(_bounds.p.x + _bounds.d.w - 1),
            static_cast<float>(_bounds.p.y + _bounds.d.h - 1));

        new haiku::NativeWindow(const_cast<app_wnd *>(this), frame, _title.c_str());

        if (!haiku::wnd_bindings.from_b(const_cast<app_wnd *>(this)))
            throw std::runtime_error("Haiku: Failed to create NativeWindow.");

        _created = true;
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        if (!_created)
            throw std::runtime_error("Haiku: Cannot show window before it is created.");

        BWindow *win = haiku::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!win)
            throw std::runtime_error("Haiku: Missing BWindow binding for app_wnd.");

        win->Show();
        invalidate();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        BWindow *win = haiku::wnd_bindings.from_b(self);

        if (auto *cache = haiku::wnd_gpx_bindings.from_a(self))
        {
            delete cache;
            haiku::wnd_gpx_bindings.unregister_by_a(self);
        }

        if (win)
        {
            haiku::wnd_bindings.unregister_by_b(self);
            if (win->Lock())
                win->Quit();
            else
                win->PostMessage(B_QUIT_REQUESTED);
        }

        _created = false;
    }
} // namespace native
