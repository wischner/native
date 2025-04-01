#include "native.h"
#include <X11/Xlib.h>
#include <iostream>

// Optional globals if needed
extern Display *g_display;
extern Window g_main_window;

namespace native
{
    int app::main_loop()
    {
        XEvent event;
        bool running = true;

        while (running)
        {
            XNextEvent(g_display, &event);
            if (event.type == KeyPress || event.type == DestroyNotify)
                running = false;
        }

        XDestroyWindow(g_display, g_main_window);
        XCloseDisplay(g_display);
        return 0;
    }
}
