//
// Declares internal OpenMotif shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace linux::openmotif
{
    // Xt callbacks carry widgets, so process-wide registries recover
    // the corresponding C++ objects during event dispatch.
    // Owns an X11 core font used by Motif.
    struct motif_font
    {
        Display *display;
        Font xfont;
        XFontStruct *metrics;
        bool owned;
    };

    struct motif_gpx
    {
        GC gc = nullptr;
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    };

    // Carries a portable menu command into an Xt callback.
    struct motif_menu_callback
    {
        native::app_wnd *owner = nullptr;
        int item_id = 0;
    };

    struct motif_menu
    {
        Widget menu_bar = nullptr;
        native::app_wnd *owner = nullptr;
        std::vector<motif_menu_callback *> callbacks;
    };

    struct motif_button
    {
        Widget widget = nullptr;
        native::button *owner = nullptr;
    };

    struct motif_text_edit
    {
        Widget widget = nullptr;
        bool multiline = false;
        bool suppress = false;
    };

    // Owns the Motif widgets used by one file-dialog session.
    struct motif_file_dialog
    {
        Widget selector = nullptr;
        Widget confirmation = nullptr;
        std::string pending_path;
    };

    extern XtAppContext app_instance;
    extern bool exit_requested;

    extern native::bindings<Widget, native::wnd *> wnd_bindings;
    extern native::bindings<Widget, native::wnd *> shell_bindings;
    extern native::bindings<Widget, native::wnd *> main_wnd_bindings;
    extern native::bindings<native::wnd *, motif_gpx *>
        wnd_gpx_bindings;
    extern native::bindings<uint32_t, motif_font *> font_bindings;
    extern native::bindings<uint32_t, motif_menu *> menu_bindings;
    extern native::bindings<native::button *, motif_button *>
        button_bindings;
    extern native::bindings<native::text_edit *, motif_text_edit *>
        text_edit_bindings;
    extern native::bindings<native::file_dialog *, motif_file_dialog *>
        file_dialog_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;
} // namespace linux::openmotif
