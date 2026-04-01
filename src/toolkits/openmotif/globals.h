#pragma once

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace motif
{
    // Platform handle for a font_t — owns an X11 core font under Motif.
    struct motiffont
    {
        Display *display;
        Font xfont;
        bool owned;
    };

    typedef struct
    {
        GC gc = nullptr;
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    } motifgpx;

    struct motifmenu {
        Widget menu_bar = nullptr;
        native::app_wnd *owner = nullptr;
    };

    struct motifbutton
    {
        Widget widget = nullptr;
        native::button *owner = nullptr;
    };

    extern XtAppContext app_instance;
    extern bool exit_requested;

    extern native::bindings<Widget, native::wnd *> wnd_bindings;
    extern native::bindings<Widget, native::wnd *> shell_bindings;
    extern native::bindings<Widget, native::wnd *> main_wnd_bindings;
    extern native::bindings<native::wnd *, motifgpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, motiffont *> font_bindings;
    extern native::bindings<uint32_t, motifmenu *> menu_bindings;
    extern native::bindings<native::button *, motifbutton *> button_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;
}
