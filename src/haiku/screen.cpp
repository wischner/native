#include <native.h>
#include <Screen.h>
#include <iostream>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        // Haiku currently only supports one screen per BScreen instance
        BScreen bscreen;
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
