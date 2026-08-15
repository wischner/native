//
// Implements the Haiku display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Application.h>
#include <Deskbar.h>
#include <Screen.h>

#include <stdexcept>

#include <native.h>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect() {
        _screens.clear();

        // BScreen needs a live app_server connection. The first screen
        // query happens before the window is created, so make
        // sure the application object exists first.
        if (!be_app && !haiku::global_app)
            haiku::global_app = new BApplication(
                "application/x-vnd.wischner-native");

        BScreen bscreen(B_MAIN_SCREEN_ID);
        if (!bscreen.IsValid())
            throw std::runtime_error(
                "Haiku: No valid screen is available.");

        BRect frame = bscreen.Frame();
        BRect usable = frame;

        BDeskbar deskbar;
        if (deskbar.IsRunning() && !deskbar.IsAutoHide()) {
            BRect deskbar_frame = deskbar.Frame();

            switch (deskbar.Location()) {
            case B_DESKBAR_TOP:
                usable.top = deskbar_frame.bottom + 1;
                break;
            case B_DESKBAR_BOTTOM:
                usable.bottom = deskbar_frame.top - 1;
                break;
            case B_DESKBAR_LEFT_TOP:
            case B_DESKBAR_LEFT_BOTTOM:
                usable.left = deskbar_frame.right + 1;
                break;
            case B_DESKBAR_RIGHT_TOP:
            case B_DESKBAR_RIGHT_BOTTOM:
                usable.right = deskbar_frame.left - 1;
                break;
            }
        }

        if (!usable.IsValid())
            usable = frame;

        rect bounds(static_cast<coord>(frame.left),
                    static_cast<coord>(frame.top),
                    static_cast<dim>(frame.Width() + 1),
                    static_cast<dim>(frame.Height() + 1));

        rect work(static_cast<coord>(usable.left),
                  static_cast<coord>(usable.top),
                  static_cast<dim>(usable.Width() + 1),
                  static_cast<dim>(usable.Height() + 1));

        _screens.emplace_back(0, bounds, work, true);

        normalize();
        return _screens;
    }

} // namespace native
