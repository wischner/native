#include <stdexcept>

#include <X11/Xlib.h>

#include <native.h>

#include "globals.h"

namespace native
{

    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        if (!motif::cached_display)
        {
            motif::cached_display = XOpenDisplay(nullptr);
            if (!motif::cached_display)
                throw std::runtime_error("Motif: No display available to detect screens.");
        }

        Display *display = motif::cached_display;
        const int screen_count = ScreenCount(display);

        for (int i = 0; i < screen_count; ++i)
        {
            Screen *scr = ScreenOfDisplay(display, i);
            rect bounds(0, 0, WidthOfScreen(scr), HeightOfScreen(scr));
            rect work_area = bounds;
            bool is_primary = (i == DefaultScreen(display));
            _screens.emplace_back(i, bounds, work_area, is_primary);
        }

        return _screens;
    }

} // namespace native
