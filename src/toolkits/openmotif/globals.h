#pragma once

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace motif
{
    // Graphics cache structure for OpenMotif (same as X11)
    typedef struct
    {
        GC gc = nullptr; // Cached X11 graphics context

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;

        // Cached font
        std::string font_name;
        Font font = 0;
    } motifgpx;

    extern native::bindings<Widget, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, motifgpx *> wnd_gpx_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;
}
