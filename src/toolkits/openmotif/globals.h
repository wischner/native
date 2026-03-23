#pragma once

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace motif
{
    typedef struct
    {
        GC gc = nullptr;
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        std::string font_name;
        Font font = 0;
    } motifgpx;

    extern XtAppContext app_instance;
    extern bool exit_requested;

    extern native::bindings<Widget, native::wnd *> wnd_bindings;
    extern native::bindings<Widget, native::wnd *> shell_bindings;
    extern native::bindings<native::wnd *, motifgpx *> wnd_gpx_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;
}
