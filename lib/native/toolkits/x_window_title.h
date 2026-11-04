//
// Declares UTF-8 window title publication for X11-based toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Xlib.h>
#include <string>

namespace native::detail
{
    // Publish ICCCM text and EWMH UTF-8 names on a realized window.
    void set_x_window_title(Display *display, Window window,
                            const std::string &title);
}
