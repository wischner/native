//
// Declares internal X11 shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <vector>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace linux::x11
{
    // Xt callbacks carry widgets, so process-wide registries recover
    // the corresponding C++ objects during event dispatch.
    extern XtAppContext app_instance;
    extern bool exit_requested;
    extern native::bindings<Widget, native::wnd *> wnd_bindings;
    extern native::bindings<Widget, native::wnd *> shell_bindings;
    extern native::bindings<Widget, native::wnd *> main_wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Platform handle for a font_t — owns an X11 core font.
    struct x11_font
    {
        Display *display;
        Font xfont;
        XFontStruct *metrics;
        bool owned;  // if true, XUnloadFont on destruction
    };

    // Internally cached values for gc and backbuffer
    struct x11_gpx
    {
        GC gc = nullptr;

        // Off-screen backbuffer (eliminates white-flash on repaint)
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    };

    extern native::bindings<native::wnd *, x11_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, x11_font *> font_bindings;

    // Carries a portable menu command into an Athena callback.
    struct xaw_menu_callback
    {
        native::app_wnd *owner = nullptr;
        int item_id = 0;
    };

    struct xaw_menu
    {
        Widget menu_bar = nullptr;
        native::app_wnd *owner = nullptr;
        std::vector<xaw_menu_callback *> callbacks;
    };

    extern native::bindings<uint32_t, xaw_menu *> menu_bindings;

    struct xaw_button
    {
        Widget widget = nullptr;
        native::button *owner = nullptr;
    };

    extern native::bindings<
        native::button *,
        xaw_button *> button_bindings;
}
