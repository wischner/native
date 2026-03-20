#pragma once

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace x11
{
    extern native::bindings<Window, native::wnd *> wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Internally cached values for gc and backbuffer
    typedef struct
    {
        GC gc = nullptr;

        // Off-screen backbuffer (eliminates white-flash on repaint)
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Cached font
        std::string font_name;
        Font font = 0;
    } x11gpx;

    extern native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings;
}
