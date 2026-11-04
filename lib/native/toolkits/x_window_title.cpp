//
// Publishes UTF-8 titles without labelling UTF-8 bytes as Latin-1.
// Classic managers receive ICCCM text; modern managers receive EWMH.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "x_window_title.h"
#include <X11/Xutil.h>

namespace native::detail
{
    void set_x_window_title(Display *display, Window window,
                            const std::string &title) {
        if (!display || !window) return;
        char *text = const_cast<char *>(title.c_str());
        XTextProperty property{};
        if (Xutf8TextListToTextProperty(display, &text, 1,
                                       XStdICCTextStyle, &property) >= Success) {
            XSetWMName(display, window, &property);
            XSetWMIconName(display, window, &property);
            if (property.value) XFree(property.value);
        }
        const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
        for (const char *name : {"_NET_WM_NAME", "_NET_WM_ICON_NAME"})
            XChangeProperty(display, window, XInternAtom(display, name, False),
                utf8, 8, PropModeReplace,
                reinterpret_cast<const unsigned char *>(title.data()),
                static_cast<int>(title.size()));
    }
}
