#include <native.h>
#include <X11/Xlib.h>
#include <iostream>

namespace x11
{
    Display *cached_display = nullptr;
}

namespace native
{

    extern std::vector<native::screen> screens;

    void screen::detect()
    {
        if (!x11::cached_display)
        {
            x11::cached_display = XOpenDisplay(nullptr);
            if (!x11::cached_display)
            {
                std::cerr << "X11: No display available to detect screens.\n";
                return;
            }
        }

        screens.clear();
        int screen_count = ScreenCount(x11::cached_display);

        for (int i = 0; i < screen_count; ++i)
        {
            Screen *scr = ScreenOfDisplay(x11::cached_display, i);
            int width = WidthOfScreen(scr);
            int height = HeightOfScreen(scr);

            rect bounds(0, 0, width, height);
            rect work_area = bounds; // X11 doesn't expose work area (WM-dependent)

            bool is_primary = (i == DefaultScreen(x11::cached_display));

            screens.emplace_back(i, bounds, work_area, is_primary);
        }
    }

} // namespace native
