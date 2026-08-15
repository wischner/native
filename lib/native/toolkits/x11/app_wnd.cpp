//
// Implements the X11 application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <X11/Xlib.h>

#include <native.h>
#include "bindings.h"
#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        Display *display = linux::x11::cached_display;
        Window window = linux::x11::wnd_bindings.handle_from_object(this);
        if (!display || !window)
            throw std::runtime_error(
                "X11: Missing window binding for app_wnd.");

        XStoreName(display, window, _title.c_str());
        XFlush(display);
    }

    void app_wnd::create() const {
        if (_created)
            return;

        if (!linux::x11::cached_display) {
            linux::x11::cached_display = XOpenDisplay(nullptr);
            if (!linux::x11::cached_display)
                throw std::runtime_error("X11: Failed to open display.");
        }

        int screen = DefaultScreen(linux::x11::cached_display);
        Window root = RootWindow(linux::x11::cached_display, screen);

        Window main_wnd = XCreateSimpleWindow(
            linux::x11::cached_display,
            root,
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h,
            1,
            BlackPixel(linux::x11::cached_display, screen),
            WhitePixel(linux::x11::cached_display, screen));

        if (!main_wnd)
            throw std::runtime_error("X11: Failed to create main window.");

        // Suppress the server-side background paint so XClearArea only
        // generates Expose events without blanking the window to white.
        // The backbuffer XCopyArea fills the window instead.
        XSetWindowBackgroundPixmap(linux::x11::cached_display, main_wnd, None);

        // Set window title
        XStoreName(linux::x11::cached_display, main_wnd, _title.c_str());

        // Subscribe to events
        XSelectInput(linux::x11::cached_display, main_wnd,
                     ExposureMask | StructureNotifyMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask);

        // Handle WM_DELETE_WINDOW
        linux::x11::wm_delete_window_atom = XInternAtom(
            linux::x11::cached_display,
            "WM_DELETE_WINDOW",
            False);
        XSetWMProtocols(
            linux::x11::cached_display,
            main_wnd,
            &linux::x11::wm_delete_window_atom,
            1);

        // Register in window binding registry
        linux::x11::wnd_bindings.register_pair(main_wnd, const_cast<app_wnd *>(this));

        // Mark as created
        _created = true;

        // Attach menu (if any was configured before create())
        const_cast<app_wnd *>(this)->menu.attach(*const_cast<app_wnd *>(this));

        // Notify creation
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        Window win = linux::x11::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (win) {
            XDestroyWindow(linux::x11::cached_display, win);
            linux::x11::wnd_bindings.unregister_by_object(self);
        }
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error("X11: Cannot show window before it's created.");

        Window main_wnd = linux::x11::wnd_bindings.handle_from_object(const_cast<app_wnd *>(this));
        if (!linux::x11::cached_display || !main_wnd)
            throw std::runtime_error(
                "X11: Missing window binding for app_wnd.");

        XMapWindow(linux::x11::cached_display, main_wnd);
        XFlush(linux::x11::cached_display);
    }

} // namespace native
