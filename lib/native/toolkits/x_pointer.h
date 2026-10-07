//
// Supplies private Xlib cursor and pointer helpers shared by X11 toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Xlib.h>
#include "../mouse_state.h"

namespace native::detail
{
    // Create a transparent cursor; the server owns copied pixmap data.
    inline Cursor invisible_x_cursor(Display *display, Window window) {
        const char empty = 0;
        Pixmap bitmap = XCreateBitmapFromData(display, window, &empty, 1, 1);
        if (!bitmap) return None;
        XColor color{};
        Cursor cursor = XCreatePixmapCursor(display, bitmap, bitmap,
                                             &color, &color, 0, 0);
        XFreePixmap(display, bitmap);
        return cursor;
    }
    // Query a native window without treating captured outside points as hover.
    inline point x_pointer_position(Display *display, Window window) {
        Window root = None, child = None;
        int rx = 0, ry = 0, x = -1, y = -1;
        unsigned mask = 0;
        XQueryPointer(display, window, &root, &child, &rx, &ry, &x, &y, &mask);
        return point(static_cast<coord>(x), static_cast<coord>(y));
    }
}
