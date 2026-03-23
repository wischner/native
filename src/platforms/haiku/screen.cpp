#include <native.h>
#include <Application.h>
#include <Screen.h>
#include <iostream>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        // BScreen needs a live app_server connection. In this backend the
        // first screen query happens before the window is created, so make
        // sure the application object exists first.
        if (!be_app && !haiku::global_app)
            haiku::global_app = new BApplication("application/x-vnd.wischner-native");

        BScreen bscreen(B_MAIN_SCREEN_ID);
        if (!bscreen.IsValid())
        {
            std::cerr << "Haiku: No valid BScreen found.\n";
            return _screens;
        }

        BRect frame = bscreen.Frame();  // Full screen bounds
        BRect usable = bscreen.Frame(); // TODO: Detect work area (excluding deskbar)

        rect bounds((int)frame.left, (int)frame.top,
                    (int)frame.Width() + 1, (int)frame.Height() + 1);

        rect work((int)usable.left, (int)usable.top,
                  (int)usable.Width() + 1, (int)usable.Height() + 1);

        _screens.emplace_back(0, bounds, work, true);

        return _screens;
    }

} // namespace native
