//
// Declares UTF-8 window title and icon name publication for X11-based
// toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Xlib.h>
#include <string>

namespace native::detail
{
    // Publish ICCCM WM_NAME and EWMH _NET_WM_NAME on a realized window.
    void set_x_window_title(Display *display, Window window,
                            const std::string &title);

    // Publish ICCCM WM_ICON_NAME and EWMH _NET_WM_ICON_NAME, the label
    // window managers show with an iconified window.
    void set_x_icon_name(Display *display, Window window,
                         const std::string &text);
}
