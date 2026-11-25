//
// Publishes UTF-8 titles and icon names without labelling UTF-8 bytes
// as Latin-1. Classic managers receive ICCCM text; modern managers
// receive EWMH.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "x_window_title.h"
#include <X11/Xutil.h>

namespace
{
    // Publish one ICCCM text property and its EWMH UTF-8 counterpart.
    void publish_name(Display *display, Window window,
                      const std::string &text,
                      void (*set_icccm)(Display *, Window, XTextProperty *),
                      const char *ewmh) {
        if (!display || !window) return;
        char *list = const_cast<char *>(text.c_str());
        XTextProperty property{};
        if (Xutf8TextListToTextProperty(display, &list, 1,
                                       XStdICCTextStyle, &property) >= Success) {
            set_icccm(display, window, &property);
            if (property.value) XFree(property.value);
        }
        XChangeProperty(display, window, XInternAtom(display, ewmh, False),
            XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace,
            reinterpret_cast<const unsigned char *>(text.data()),
            static_cast<int>(text.size()));
    }
}

namespace native::detail
{
    void set_x_window_title(Display *display, Window window,
                            const std::string &title) {
        publish_name(display, window, title, XSetWMName, "_NET_WM_NAME");
    }

    void set_x_icon_name(Display *display, Window window,
                         const std::string &text) {
        publish_name(display, window, text, XSetWMIconName,
                     "_NET_WM_ICON_NAME");
    }
}
