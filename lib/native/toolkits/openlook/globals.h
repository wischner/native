//
// Declares private process-wide XView bindings and backend resources
// used by the Linux OPEN LOOK implementation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <bindings.h>
#include <native.h>

#include <X11/Xlib.h>
#include <xview/file_chsr.h>
#include <xview/frame.h>
#include <xview/openmenu.h>
#include <xview/panel.h>
#include <xview/sel_pkg.h>

#include "xview_compat.h"

namespace linux::openlook
{
    // Owns the XView resources representing one portable top level.
    struct openlook_window
    {
        Frame frame = XV_NULL;
        Panel content = XV_NULL;
        Xv_Window paint_window = XV_NULL;
        int menu_height = 0;
    };

    // Owns Xlib drawing state and the window backbuffer.
    struct openlook_gpx
    {
        GC gc = nullptr;
        Pixmap backbuffer = 0;
        int buffer_width = 0;
        int buffer_height = 0;
        native::rgba current_ink = 0xffffffff;
        int current_thickness = -1;
    };

    // Owns an X11 core font selected through XView resources.
    struct openlook_font
    {
        Display *display = nullptr;
        Font xfont = 0;
        XFontStruct *metrics = nullptr;
        bool owned = false;
    };

    // Routes one native menu item back to a portable command.
    struct openlook_menu_callback
    {
        native::app_wnd *owner = nullptr;
        int item_id = 0;
    };

    // Owns the native panel menu bar and command menus.
    struct openlook_menu
    {
        Panel bar = XV_NULL;
        native::app_wnd *owner = nullptr;
        std::vector<Menu> menus;
        std::vector<openlook_menu_callback *> callbacks;
    };

    // Owns one native XView text item and callback guard.
    struct openlook_text_edit
    {
        Panel_item item = XV_NULL;
        bool multiline = false;
        bool suppress = false;
        bool all_selected = false;
    };

    // Owns one asynchronous standard XView file chooser.
    struct openlook_file_dialog
    {
        File_chooser chooser = XV_NULL;
        native::file_dialog *dialog = nullptr;
        bool save = false;
    };

    extern bool initialized;
    extern bool exit_requested;
    extern Display *cached_display;
    extern Frame main_frame;

    extern native::bindings<Xv_opaque, native::wnd *> wnd_bindings;
    extern native::bindings<Xv_opaque, native::app_wnd *>
        frame_bindings;
    extern native::bindings<native::app_wnd *, openlook_window *>
        window_bindings;
    extern native::bindings<native::wnd *, openlook_gpx *>
        wnd_gpx_bindings;
    extern native::bindings<std::uint32_t, openlook_font *>
        font_bindings;
    extern native::bindings<std::uint32_t, openlook_menu *>
        menu_bindings;
    extern native::bindings<native::text_edit *, openlook_text_edit *>
        text_edit_bindings;
    extern native::bindings<
        const native::file_dialog *, openlook_file_dialog *>
        file_dialog_bindings;

    // Return the content panel belonging to a created parent window.
    Panel parent_panel(native::wnd *control);

    // Return the native top-level state for a portable window.
    openlook_window *window_state(native::app_wnd *window);

    // Permit input or restore focus to the active modal dialog.
    bool permit_input(native::wnd *window);

    // Repaint a window synchronously without clearing its live Panel.
    void repaint_window(native::app_wnd *window,
                        const native::rect &area);

    // Return the X drawable used for portable window painting.
    Window drawable(native::wnd *window);
} // namespace linux::openlook
