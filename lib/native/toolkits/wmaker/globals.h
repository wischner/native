//
// Declares private WINGs bindings and Window Maker backend resources.
// Native handles remain below the public Native API boundary.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>
#include <WINGs/WINGs.h>

#include <bindings.h>

#include "../../wnd_peer.h"
#include <native.h>

namespace linux::wmaker
{
    inline constexpr int menu_bar_height = 24;

    // Owns the WINGs resources representing one portable top level.
    struct window_state
    {
        WMWindow *window = nullptr;
        int menu_height = 0;
    };

    // Stretch a window's menu strip across a new window width.
    void resize_menu_bar(native::app_wnd *owner, int width);

    // Owns one buffered Xlib graphics context.
    struct window_graphics
    {
        GC gc = nullptr;
        Pixmap backbuffer = None;
        int width = 0;
        int height = 0;
        native::rgba current_ink = 0xffffffff;
        int current_thickness = -1;
    };

    // Borrows a WINGs font retained by the toolkit or this binding.
    struct native_font
    {
        WMFont *font = nullptr;
        bool owned = false;
    };

    struct native_menu;

    // Routes one persistent menu-bar title to its popup model.
    struct menu_callback
    {
        native_menu *menu = nullptr;
        WMFrame *title_widget = nullptr;
        std::size_t top_index = 0;
        WMFont *font = nullptr;
        bool hot = false;
    };

    // Owns a row of WINGs pull-down buttons.
    struct native_menu
    {
        native::app_wnd *owner = nullptr;

        // Fills the menu strip from edge to edge behind the buttons.
        // The buttons only cover their own labels, and nothing else
        // paints that row, so without this the window background
        // shows through beside them.
        WMFrame *background = nullptr;
        // One native dark rule separating the menu strip from client content.
        WMFrame *separator = nullptr;
        std::vector<WMFrame *> titles;
        std::vector<menu_callback *> callbacks;
        std::vector<native::main_menu::top_entry> tops;
        Window popup = None;
        int open_top = -1;
        int hot_item = -1;
        int popup_width = 0;
        int popup_height = 0;
        int popup_x = 0;
        int popup_y = 0;
        int item_height = 0;
    };

    // Owns a WINGs single-line or multiline text widget adapter.
    struct native_text_edit
    {
        WMWidget *widget = nullptr;
        WMTextField *field = nullptr;
        WMText *text = nullptr;
        WMTextFieldDelegate delegate = {};
        bool suppress = false;
        bool all_selected = false;
    };
    struct native_combo_box
    {
        WMFrame *frame = nullptr;
        WMPopUpButton *popup = nullptr;
        WMTextField *field = nullptr;
        WMFrame *arrow = nullptr;
        WMTextFieldDelegate delegate = {};
        WMHandlerID arrow_timer = nullptr;
        bool arrow_pressed = false;
        bool arrow_overlay = false;
        bool suppress = false;
    };

    // Apply portable combo style and geometry to its native children.
    void configure_combo_box(native::combo_box &owner,
                             native_combo_box &state);

    struct native_collection
    {
        WMFrame *frame = nullptr;
        WMScroller *vertical_scroller = nullptr;
        WMScroller *horizontal_scroller = nullptr;
        bool vertical_visible = false;
        bool horizontal_visible = false;
        Time last_click = 0;
        int last_item = -1;
        native::table_row_id last_row =
            native::invalid_table_row_id;
        native::tree_item_id last_tree_item =
            native::invalid_tree_item_id;
    };

    struct native_tab_view
    {
        WMTabView *tabs = nullptr;
        native_collection *portable = nullptr;
        std::vector<WMFrame *> pages;
        std::vector<WMTabViewItem *> items;
        WMTabViewDelegate delegate = {};
        bool suppress = false;
    };

    struct native_split_view
    {
        WMSplitView *split = nullptr;
        WMFrame *first = nullptr;
        WMFrame *second = nullptr;
        bool applying_ratio = false;
    };

    extern bool initialized;
    extern bool exit_requested;
    extern Display *display;
    extern WMScreen *screen;
    extern WMColor *list_selection_background;
    extern WMColor *list_selection_text;

    extern native::bindings<WMWidget *, native::wnd *> wnd_bindings;
    inline constexpr native::detail::peer_bindings<
        native::app_wnd *, window_state *> window_bindings;
    inline constexpr native::detail::peer_bindings<
        native::wnd *, window_graphics *> graphics_bindings;
    extern native::bindings<std::uint32_t, native_font *>
        font_bindings;
    extern native::bindings<std::uint32_t, native_menu *>
        menu_bindings;
    inline constexpr native::detail::peer_bindings<
        native::text_edit *, native_text_edit *> text_edit_bindings;
    inline constexpr native::detail::peer_bindings<
        native::combo_box *, native_combo_box *> combo_box_bindings;
    inline constexpr native::detail::peer_bindings<
        native::accordion *, native_collection *> accordion_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tab_view *, native_tab_view *> tab_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::split_view *, native_split_view *> split_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::icon_view *, native_collection *> icon_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tree_view *, native_collection *> tree_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::table_view *, native_collection *> table_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::code_edit *, native_collection *> code_edit_bindings;
    inline constexpr native::detail::peer_bindings<
        native::canvas *, native_collection *> canvas_bindings;

    // Initialize the process-wide display and WINGs application screen.
    void initialize();

    // Return a created parent's WINGs widget or throw.
    WMWidget *parent_widget(native::wnd *control);

    // Return the state belonging to a portable top-level window.
    window_state *state(native::app_wnd *window);

    // Return the X drawable used by a portable window.
    Window drawable(native::wnd *window);

    // Translate a control position into its native parent coordinates.
    native::point control_position(const native::wnd *control);

    // Return a reachable top-level position with its title on screen.
    native::point constrain_position(const native::point &preferred,
                                     const native::size &dimensions);

    // Permit an input callback or raise the active modal branch.
    bool permit_input(native::wnd *window);

    // Schedule a buffered repaint without clearing the live window.
    void schedule_repaint(native::app_wnd *window,
                          const native::rect &area);

    // Run a portable callback after WINGs finishes the current event.
    void defer(std::function<void()> callback);

    // Drain callbacks queued by WINGs actions and event handlers.
    void dispatch_deferred();

    // Consume raw events belonging to the persistent application menu or
    // its keyboard mnemonics/accelerators before WINGs dispatch.
    bool handle_menu_event(XEvent &event);

    // Remember the focused top level for application-menu shortcuts.
    void activate_menu_owner(native::app_wnd *owner);
} // namespace linux::wmaker
