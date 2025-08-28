#pragma once

#include <native.h>
#include <bindings.h>

namespace x11
{
    extern native::bindings<Window, native::wnd *> wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Internally cached values for gc
    typedef struct 
    {
        GC gc = nullptr;  // Now it owns the GC!

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;

        // Cached font
        std::string font_name;
        Font font = 0;
    } x11gpx;

    extern native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings;
}
