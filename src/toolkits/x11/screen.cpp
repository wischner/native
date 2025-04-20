#include <stdexcept>

#include <X11/Xlib.h>

#include <native.h>
#include "globals.h"

namespace native
{

    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        if (!x11::cached_display)
        {
            x11::cached_display = XOpenDisplay(nullptr);
            if (!x11::cached_display)
                throw std::runtime_error("X11: No display available to detect screens.");
        }

        int screen_count = ScreenCount(x11::cached_display);

        for (int i = 0; i < screen_count; ++i)
        {
            Screen *scr = ScreenOfDisplay(x11::cached_display, i);
            int width = WidthOfScreen(scr);
            int height = HeightOfScreen(scr);

            rect bounds(0, 0, width, height);
            rect work_area = bounds; // X11 doesn't expose work area (WM-dependent)

            bool is_primary = (i == DefaultScreen(x11::cached_display));

            _screens.emplace_back(i, bounds, work_area, is_primary);
        }

        return _screens;
    }

} // namespace native
