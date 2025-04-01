#include "native.h"
#include <X11/Xlib.h>
#include <iostream>

// Define the global handles expected by app.cpp
Display *g_display = nullptr;
Window g_main_window = 0;

namespace native
{
    void wnd::create() const
    {
        g_display = XOpenDisplay(nullptr);
        if (!g_display)
        {
            std::cerr << "Failed to open X display." << std::endl;
            return;
        }

        int screen = DefaultScreen(g_display);
        unsigned long white = WhitePixel(g_display, screen);
        unsigned long black = BlackPixel(g_display, screen);

        g_main_window = XCreateSimpleWindow(
            g_display,
            DefaultRootWindow(g_display),
            x(), y(), width(), height(),
            1,
            black, white);

        // Set title (optional, since it's in app_wnd, we could pass it via dynamic_cast)
        XStoreName(g_display, g_main_window, "X11 Window");

        // Select input events to watch
        XSelectInput(g_display, g_main_window,
                     ExposureMask | KeyPressMask | StructureNotifyMask);
    }

    void wnd::show() const
    {
        if (!g_display || !g_main_window)
        {
            std::cerr << "X11: Cannot show window — not created." << std::endl;
            return;
        }

        XMapWindow(g_display, g_main_window);
        XFlush(g_display);
    }
}
