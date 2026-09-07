//
// Declares native X window border masking without replacing widgets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Xlib.h>
#include <native/border.h>

namespace native::detail
{
    // Keep selected native edges; outside is the X core border width.
    void shape_border(Display *display, Window window, border_sides sides,
                      int inside, int outside = 0);
}
