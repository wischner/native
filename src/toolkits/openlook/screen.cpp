#include <stdexcept>

#include <X11/Xlib.h>
#ifdef coord
#undef coord
#endif

#include <native.h>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        Display *display = openlook::cached_display;
        bool temporary_display = false;

        // app::run() calls screen::detect() before app_wnd::create(), so for
        // OpenLook use a short-lived probe display and let XView own the real
        // connection later in app_wnd::create().
        if (!display)
        {
            display = XOpenDisplay(nullptr);
            if (!display)
                throw std::runtime_error("OpenLook: No display available to detect screens.");
            temporary_display = true;
        }

        const int screen_count = ScreenCount(display);
        for (int i = 0; i < screen_count; ++i)
        {
            Screen *scr = ScreenOfDisplay(display, i);
            rect bounds(0, 0, WidthOfScreen(scr), HeightOfScreen(scr));
            rect work_area = bounds;
            bool is_primary = (i == DefaultScreen(display));
            _screens.emplace_back(i, bounds, work_area, is_primary);
        }

        if (temporary_display)
            XCloseDisplay(display);

        return _screens;
    }

} // namespace native
