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

    struct motif_collection
    {
        Widget widget = nullptr;
        Widget content = nullptr;
        Widget vertical_scrollbar = nullptr;
        Widget horizontal_scrollbar = nullptr;
        std::vector<Widget> items;
        std::vector<native::table_row_id> row_ids;
        std::vector<native::tree_item_id> tree_ids;
        std::vector<Widget> group_items;
        std::vector<native::table_group_id> group_ids;
        std::vector<Pixmap> pixmaps;
        Pixmap collapsed_tree_pixmap = XmUNSPECIFIED_PIXMAP;
        Pixmap expanded_tree_pixmap = XmUNSPECIFIED_PIXMAP;
        bool native_table = false;
        bool native_tree = false;
        bool native_scroll_callbacks = false;
        bool synchronizing_scrollbars = false;
        bool suppress = false;
        Time last_click = 0;
        int last_item = -1;
        native::table_row_id last_row =
            native::invalid_table_row_id;
        native::tree_item_id last_tree_item =
            native::invalid_tree_item_id;
    };

    struct motif_tab_view
    {
        Widget notebook = nullptr;
        std::vector<Widget> page_frames;
        std::vector<Widget> pages;
        std::vector<Widget> tabs;
        std::vector<Pixmap> label_pixmaps;
        Dimension frame_shadow_thickness = 0;
        bool suppress = false;
    };

    struct motif_split_view
    {
        Widget paned = nullptr;
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
    extern native::bindings<native::list *, Widget>
        list_content_bindings;
    extern native::bindings<native::combo_box *, Widget>
        combo_box_bindings;
    extern native::bindings<native::accordion *, motif_collection *>
        accordion_bindings;
    extern native::bindings<native::tab_view *, motif_tab_view *>
        tab_view_bindings;
    extern native::bindings<native::split_view *, motif_split_view *>
        split_view_bindings;
    extern native::bindings<native::icon_view *, motif_collection *>
        icon_view_bindings;
    extern native::bindings<native::tree_view *, motif_collection *>
        tree_view_bindings;
    extern native::bindings<native::table_view *, motif_collection *>
        table_view_bindings;
    extern native::bindings<native::code_edit *, motif_collection *>
        code_edit_bindings;
    extern native::bindings<native::canvas *, motif_collection *>
        canvas_bindings;
    extern native::bindings<native::file_dialog *, motif_file_dialog *>
        file_dialog_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Return the native parent for a child, including notebook pages.
    Widget parent_widget(native::wnd *child);
} // namespace linux::openmotif
