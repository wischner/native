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

#include "../../wnd_peer.h"
#include <native.h>

#include <X11/Xlib.h>
#include <xview/canvas.h>
#include <xview/file_chsr.h>
#include <xview/frame.h>
#include <xview/openmenu.h>
#include <xview/panel.h>
#include <xview/scrollbar.h>
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
        bool repaint_pending = false;
        bool items_repaint_pending = false;
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
    struct openlook_combo_box
    {
        Panel_item text = XV_NULL;
        Panel_item choice = XV_NULL;
        Menu menu = XV_NULL;
        bool suppress = false;
    };

    struct openlook_collection
    {
        Panel panel = XV_NULL;
        // Tabs keep page controls on a sibling Panel so the custom-painted
        // tab canvas cannot overdraw native Panel items.
        Panel content_panel = XV_NULL;
        Xv_Window paint_window = XV_NULL;
        Scrollbar vertical_scrollbar = XV_NULL;
        Scrollbar horizontal_scrollbar = XV_NULL;
        bool synchronizing_scrollbars = false;
        bool repaint_pending = false;
        Time last_click = 0;
        int last_item = -1;
        native::table_row_id last_row =
            native::invalid_table_row_id;
        native::tree_item_id last_tree_item =
            native::invalid_tree_item_id;
    };

    struct openlook_split_view
    {
        Panel host = XV_NULL;
        Panel first = XV_NULL;
        Panel second = XV_NULL;
        bool dragging = false;
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
    inline constexpr native::detail::peer_bindings<
        native::app_wnd *, openlook_window *> window_bindings;
    inline constexpr native::detail::peer_bindings<
        native::wnd *, openlook_gpx *> wnd_gpx_bindings;
    extern native::bindings<std::uint32_t, openlook_font *>
        font_bindings;
    extern native::bindings<std::uint32_t, openlook_menu *>
        menu_bindings;
    inline constexpr native::detail::peer_bindings<
        native::text_edit *, openlook_text_edit *> text_edit_bindings;
    inline constexpr native::detail::peer_bindings<
        native::combo_box *, openlook_combo_box *> combo_box_bindings;
    inline constexpr native::detail::peer_bindings<
        native::accordion *, openlook_collection *> accordion_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tab_view *, openlook_collection *> tab_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::split_view *, openlook_split_view *> split_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::icon_view *, openlook_collection *> icon_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tree_view *, openlook_collection *> tree_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::table_view *, openlook_collection *> table_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::code_edit *, openlook_collection *> code_edit_bindings;
    inline constexpr native::detail::peer_bindings<
        native::canvas *, openlook_collection *> canvas_bindings;
    extern native::bindings<Xv_Window, native::wnd *>
        collection_paint_bindings;
    extern native::bindings<
        const native::file_dialog *, openlook_file_dialog *>
        file_dialog_bindings;

    // Return the content panel belonging to a created parent window.
    Panel parent_panel(native::wnd *control);

    // Return the native top-level state for a portable window.
    openlook_window *window_state(native::app_wnd *window);

    // Permit input or restore focus to the active modal dialog.
    bool permit_input(native::wnd *window);

    // Return the exact menu-mark glyph dimensions recorded by OLGX.
    native::size menu_mark_dimensions(const void *information);

    //
    // Size a Panel item's label so the item occupies a width.
    //
    // Parameters:
    //      item        - Panel item to size.
    //      width       - Width the item should occupy, in pixels.
    //
    // Notes:
    //      An XView panel item sizes itself to its label and ignores
    //      XV_WIDTH, so controls keep their natural width however
    //      their bounds are set: buttons placed side by side overlap,
    //      and a layout resizing its children has no visible effect.
    //      PANEL_LABEL_WIDTH is honoured, but it sets the label area
    //      rather than the item, so the toolkit's own border is added
    //      on top. The item is measured after the label is set and
    //      the difference taken off, which lands the item on the
    //      requested width whatever that border costs.
    //
    void fit_item_width(Xv_opaque item, native::dim width);

    // Coalesce repainting into the next notifier turn, without clearing
    // the live Panel or reentering application constructors/layout.
    void repaint_window(native::app_wnd *window,
                        const native::rect &area);

    // Return the X drawable used for portable window painting.
    Window drawable(native::wnd *window);
} // namespace linux::openlook
