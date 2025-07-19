#include <stdexcept>
#include <iostream>

#include <X11/Xlib.h>
#include <Xm/Xm.h>

#include <native.h>
#include "bindings.h"
#include "globals.h"

namespace native
{
    app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _title(std::move(title))
    {
        // Additional initialization if needed
    }

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;

        if (_created)
        {
            // Update the title of the native window if already created
            Display *display = x11::cached_display;
            Window win = x11::wnd_bindings.from_b(this);
            XStoreName(display, win, _title.c_str());
            XFlush(display);
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

        if (!x11::cached_display)
        {
            x11::cached_display = XOpenDisplay(nullptr);
            if (!x11::cached_display)
                throw std::runtime_error("X11: Failed to open display.");
        }

        int screen = DefaultScreen(x11::cached_display);
        Window root = RootWindow(x11::cached_display, screen);

        Window main_wnd = XCreateSimpleWindow(
            x11::cached_display,
            root,
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h,
            1,
            BlackPixel(x11::cached_display, screen),
            WhitePixel(x11::cached_display, screen));

        if (!main_wnd)
            throw std::runtime_error("X11: Failed to create main window.");

        // Set window title
        XStoreName(x11::cached_display, main_wnd, title().c_str());

        // Subscribe to events
        XSelectInput(x11::cached_display, main_wnd,
                     ExposureMask | StructureNotifyMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask);

        // Handle WM_DELETE_WINDOW
        x11::wm_delete_window_atom = XInternAtom(x11::cached_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(x11::cached_display, main_wnd, &x11::wm_delete_window_atom, 1);

        // Register in window binding registry
        x11::wnd_bindings.register_pair(main_wnd, const_cast<app_wnd *>(this));

        // Mark as created
        _created = true;

        // Notify creation
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        auto *aw_this = const_cast<app_wnd *>(this);   // Step 1
        wnd *self = static_cast<wnd *>(aw_this);    // Step 2

        Window win = x11::wnd_bindings.from_b(self);
        if (win)
        {
            if (auto *cache = x11::wnd_gpx_bindings.from_a(self))
            {
                if (cache->gc)
                    XFreeGC(x11::cached_display, cache->gc);
                delete cache;
                x11::wnd_gpx_bindings.unregister_by_a(self);
            }

            XDestroyWindow(x11::cached_display, win);
            x11::wnd_bindings.unregister_by_b(self);
        }

        _created = false;
    }

    void app_wnd::show() const
    {
        if (!_created)
            throw std::runtime_error("X11: Cannot show window before it's created.");

        Window main_wnd = x11::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        XMapWindow(x11::cached_display, main_wnd);
        XFlush(x11::cached_display);
    }

} // namespace native
