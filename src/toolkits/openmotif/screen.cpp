#include <native.h>
#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <iostream>

namespace motif
{
    Display *cached_display = nullptr;
}

namespace native
{

    extern std::vector<native::screen> screens;

    void screen::detect()
    {
        if (!motif::cached_display)
        {
            motif::cached_display = XOpenDisplay(nullptr);
            if (!motif::cached_display)
            {
                std::cerr << "Motif: No display available to detect screens.\n";
                return;
            }
        }

        screens.clear();
        int screen_count = ScreenCount(motif::cached_display);
        for (int i = 0; i < screen_count; ++i)
        {
            Screen *scr = ScreenOfDisplay(motif::cached_display, i);
            int w = WidthOfScreen(scr);
            int h = HeightOfScreen(scr);
            rect bounds(0, 0, w, h);
            rect work_area = bounds;
            bool is_primary = (i == DefaultScreen(motif::cached_display));
            screens.emplace_back(i, bounds, work_area, is_primary);
        }
    }

    std::vector<screen> &get_screens()
    {
        return native::screens;
    }
} // namespace native
