#include <native.h>
#include <bindings.h>
#include <X11/Xlib.h>
#include <iostream>

namespace x11
{
    // Reuse the cached display from screen.cpp
    extern Display *cached_display;

    // Binding between X11 windows and native::wnd instances
    native::bindings<Window, native::wnd *> wnd_bindings;
}

namespace native
{
    void app_wnd::create() const
    {
        if (!x11::cached_display)
        {
            x11::cached_display = XOpenDisplay(nullptr);
            if (!x11::cached_display)
            {
                std::cerr << "X11: Failed to open display.\n";
                return;
            }
        }

        int screen = DefaultScreen(x11::cached_display);
        Window root = RootWindow(x11::cached_display, screen);

        Window main_wnd = XCreateSimpleWindow(
            x11::cached_display,
            root,
            x(), y(), width(), height(),
            1,
            BlackPixel(x11::cached_display, screen),
            WhitePixel(x11::cached_display, screen));

        if (!main_wnd)
        {
            std::cerr << "X11: Failed to create main window.\n";
            return;
        }

        // Set window title.
        XStoreName(x11::cached_display, main_wnd, title().c_str());

        // Subscribe to events.
        XSelectInput(x11::cached_display,
                     main_wnd,
                     ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask);

        // Register in window binding registry.
        x11::wnd_bindings.register_pair(main_wnd, const_cast<app_wnd *>(this));
    }

    void app_wnd::show() const
    {
        Window main_wnd = x11::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!main_wnd)
        {
            std::cerr << "X11: Can't show window, not created.\n";
            return;
        }

        XMapWindow(x11::cached_display, main_wnd);
        XFlush(x11::cached_display);
    }
}
