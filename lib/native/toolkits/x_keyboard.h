//
// Declares private X11 physical keyboard dispatch shared by Linux toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <X11/Xlib.h>
#include <native/wnd.h>
namespace native::detail
{
    // Normalize XKB physical names, repeat pairs and focus on one client.
    bool dispatch_x_keyboard(wnd &owner, XEvent &event);
}
