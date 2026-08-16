//
// Declares an independently positioned owned window that leaves its
// owner interactive.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "owned_wnd.h"

namespace native
{
    // Represents a modeless owned top-level window.
    class modeless_wnd : public owned_wnd
    {
    public:
        // Construct a modeless window from scalar screen bounds.
        modeless_wnd(app_wnd &owner,
                     std::string title,
                     coord x = 100,
                     coord y = 100,
                     dim width = 640,
                     dim height = 480);

        // Construct a modeless window from screen position and size.
        modeless_wnd(app_wnd &owner,
                     const std::string &title,
                     const point &position,
                     const size &dimensions);

        // Construct a modeless window from complete screen bounds.
        modeless_wnd(app_wnd &owner,
                     const std::string &title,
                     const rect &bounds);
    };
} // namespace native
