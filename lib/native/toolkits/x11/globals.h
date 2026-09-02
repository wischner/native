//
// Declares internal X11 shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <filesystem>
#include <string>
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
        bool owned; // if true, XUnloadFont on destruction
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
        Widget event_widget = nullptr;
        native::app_wnd *owner = nullptr;
        std::vector<xaw_menu_callback *> callbacks;
    };

    extern native::bindings<uint32_t, xaw_menu *> menu_bindings;

    struct xaw_button
    {
        Widget widget = nullptr;
        native::button *owner = nullptr;
    };

    extern native::bindings<native::button *, xaw_button *>
        button_bindings;

    struct xaw_list
    {
        Widget widget = nullptr;
        std::vector<std::string> labels;
        std::vector<char *> pointers;
    };
    struct xaw_combo_callback
    {
        native::combo_box *owner = nullptr;
        int index = -1;
    };
    struct xaw_combo_box
    {
        Widget root = nullptr;
        Widget text = nullptr;
        Widget button = nullptr;
        Widget menu = nullptr;
        std::vector<xaw_combo_callback *> callbacks;
        bool suppress = false;
    };

    extern native::bindings<native::list *, xaw_list *> list_bindings;
    extern native::bindings<native::combo_box *, xaw_combo_box *>
        combo_box_bindings;

    struct xaw_collection
    {
        Widget widget = nullptr;
        Time last_click = 0;
        int last_item = -1;
        native::table_row_id last_row =
            native::invalid_table_row_id;
        native::tree_item_id last_tree_item =
            native::invalid_tree_item_id;
    };

    extern native::bindings<native::accordion *, xaw_collection *>
        accordion_bindings;
    extern native::bindings<native::tab_view *, xaw_collection *>
        tab_view_bindings;

    struct xaw_split_view
    {
        Widget paned = nullptr;
        Widget first = nullptr;
        Widget second = nullptr;
        bool suppress = false;
    };

    extern native::bindings<native::split_view *, xaw_split_view *>
        split_view_bindings;
    extern native::bindings<native::icon_view *, xaw_collection *>
        icon_view_bindings;
    extern native::bindings<native::tree_view *, xaw_collection *>
        tree_view_bindings;
    extern native::bindings<native::table_view *, xaw_collection *>
        table_view_bindings;
    extern native::bindings<native::code_edit *, xaw_collection *>
        code_edit_bindings;

    struct xaw_text_edit
    {
        Widget widget = nullptr;
        Widget source = nullptr;
        bool suppress = false;
    };

    extern native::bindings<native::text_edit *, xaw_text_edit *>
        text_edit_bindings;

    // Owns one asynchronous Athena file-browser widget hierarchy.
    struct xaw_file_dialog
    {
        // Destroy the complete Athena widget hierarchy, if created.
        ~xaw_file_dialog();

        native::file_dialog *dialog = nullptr;
        Widget shell = nullptr;
        Widget directory_label = nullptr;
        Widget list = nullptr;
        Widget path_edit = nullptr;
        std::filesystem::path directory;
        std::vector<std::string> labels;
        std::vector<String> label_pointers;
        std::vector<std::filesystem::path> paths;
        int last_selection = -1;
        Time last_selection_time = 0;
        std::string suggested_name;
        std::string default_extension;
        std::string pending_overwrite;
        bool save = false;
        bool confirm_overwrite = true;
    };

    extern native::bindings<
        const native::file_dialog *, xaw_file_dialog *>
        file_dialog_bindings;
} // namespace linux::x11
