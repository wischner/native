#include <native.h>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect()
    {
        _screens.clear();
        rect desktop = gemix::desktop_rect();
        _screens.emplace_back(0, desktop, desktop, true);
        return _screens;
    }
}
