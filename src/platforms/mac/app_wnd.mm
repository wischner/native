#include <stdexcept>
#include <utility>

#include <AppKit/AppKit.h>

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
            NSWindow *win = mac::wnd_bindings.from_b(const_cast<app_wnd *>(this));
            if (win)
                [win setTitle:[NSString stringWithUTF8String:_title.c_str()]];
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

        const rect b = bounds();
        new mac::NativeWindow(
            const_cast<app_wnd *>(this),
            _title.c_str(),
            b.p.x,
            b.p.y,
            static_cast<int>(b.d.w),
            static_cast<int>(b.d.h));

        if (!mac::wnd_bindings.from_b(const_cast<app_wnd *>(this)))
            throw std::runtime_error("macOS: Failed to create NativeWindow.");

        _created = true;
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        if (!_created)
            throw std::runtime_error("macOS: Cannot show window before it is created.");

        NSWindow *win = mac::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!win)
            throw std::runtime_error("macOS: Missing NSWindow binding for app_wnd.");

        [win makeKeyAndOrderFront:nil];
        if (mac::global_app)
            [mac::global_app activateIgnoringOtherApps:YES];
        invalidate();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<app_wnd *>(this);
        NSWindow *win = mac::wnd_bindings.from_b(self);

        if (auto *cache = mac::wnd_gpx_bindings.from_a(self))
        {
            delete cache;
            mac::wnd_gpx_bindings.unregister_by_a(self);
        }

        if (win)
        {
            mac::wnd_bindings.unregister_by_b(self);
            [win close];
        }

        _created = false;

        if (self == app::main_wnd() && mac::global_app)
            [mac::global_app stop:nil];
    }
} // namespace native
