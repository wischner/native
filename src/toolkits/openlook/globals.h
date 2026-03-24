#pragma once

#include <string>

#include <X11/Xlib.h>

#include <xview/xview.h>
#ifdef coord
#undef coord
#endif

#include <native.h>
#include <bindings.h>

namespace openlook
{
    typedef struct
    {
        GC gc = nullptr;

        // Off-screen backbuffer (eliminates white flash on repaint)
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Cached font
        std::string font_name;
        Font font = 0;
    } openlookgpx;

    extern bool xview_initialized;
    extern bool exit_requested;

    extern native::bindings<Xv_opaque, native::wnd *> frame_bindings;
    extern native::bindings<Xv_opaque, native::wnd *> canvas_bindings;
    extern native::bindings<native::wnd *, openlookgpx *> wnd_gpx_bindings;

    extern Display *cached_display;
}
