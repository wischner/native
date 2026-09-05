//
// Declares internal macOS shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#import <Cocoa/Cocoa.h>
#include <unordered_map>
#include <vector>
#include <native.h>
#include <bindings.h>

#include "../../wnd_peer.h"

namespace mac
{
    // AppKit delegates carry objects, so process-wide registries
    // recover the corresponding C++ objects during event dispatch.
    // Platform handle for a font_t — retains an NSFont.
    struct mac_font
    {
        NSFont *ns_font;
    };

    // Graphics cache structure for macOS Cocoa/Quartz
    struct mac_gpx
    {
        NSView *view = nullptr; // Cached NSView for drawing
        NSGraphicsContext *context = nullptr; // Cached graphics context

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    struct mac_menu
    {
        NSMenu *ns_menu = nil;
        native::app_wnd *owner = nullptr;
    };

    struct mac_button
    {
        NSButton *ns_button = nil;
        id target = nil;
        native::button *owner = nullptr;
    };

    // Owns the AppKit objects used by one text editor.
    struct mac_text_edit
    {
        NSTextField *field = nil;
        NSScrollView *scroll = nil;
        NSTextView *text_view = nil;
        id delegate = nil;
        bool suppress = false;
    };

    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, id> delegate_bindings;
    inline constexpr native::detail::peer_bindings<
        native::wnd *, mac_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, mac_font *> font_bindings;
    extern native::bindings<uint32_t, mac_menu *> menu_bindings;
    inline constexpr native::detail::peer_bindings<
        native::button *, mac_button *> button_bindings;
    inline constexpr native::detail::peer_bindings<
        native::text_edit *, mac_text_edit *> text_edit_bindings;

    struct mac_check
    {
        NSButton *button = nil;
        id target = nil;
    };
    struct mac_radio
    {
        NSButton *button = nil;
        id target = nil;
    };
    struct mac_list
    {
        NSScrollView *scroll = nil;
        NSTableView *table = nil;
        id adapter = nil;
    };
    struct mac_combo_box
    {
        NSComboBox *combo = nil;
        id delegate = nil;
        bool suppress = false;
    };
    struct mac_accordion
    {
        NSScrollView *scroll = nil;
        NSStackView *stack = nil;
        id target = nil;
        std::vector<NSButton *> headers;
    };
    struct mac_tab_view
    {
        NSTabView *view = nil;
        id delegate = nil;
        bool suppress = false;
    };
    struct mac_split_view
    {
        NSSplitView *view = nil;
        NSView *first = nil;
        NSView *second = nil;
        id delegate = nil;
        bool suppress = false;
    };
    struct mac_icon_view
    {
        NSScrollView *scroll = nil;
        NSCollectionView *collection = nil;
        NSCollectionViewFlowLayout *layout = nil;
        id adapter = nil;
        std::vector<NSImage *> images;
        bool suppress = false;
    };
    struct mac_tree_view
    {
        NSScrollView *scroll = nil;
        NSOutlineView *outline = nil;
        id adapter = nil;
        std::unordered_map<native::tree_item_id, NSNumber *> items;
        std::unordered_map<native::tree_item_id, NSImage *> images;
        bool suppress = false;
    };
    struct mac_table_view
    {
        NSScrollView *scroll = nil;
        NSTableView *table = nil;
        NSTableHeaderView *header = nil;
        id adapter = nil;
        CGFloat default_row_height = 0;
        std::unordered_map<const native::img *, NSImage *> images;
        bool suppress = false;
    };
    struct mac_code_edit
    {
        NSView *view = nil;
    };

    // A structural panel host and a paintable canvas each own one
    // plain child view; nothing else about them is backend state.
    struct mac_surface
    {
        NSView *view = nil;
    };

    // Return the child-host resource carried by each control state.
    inline void *peer_content(mac_button *state) {
        return state ? reinterpret_cast<void *>(state->ns_button) : nullptr;
    }

    inline void *peer_content(mac_text_edit *state) {
        if (!state)
            return nullptr;
        NSView *view = state->scroll
                           ? static_cast<NSView *>(state->scroll)
                           : static_cast<NSView *>(state->field);
        return reinterpret_cast<void *>(view);
    }

    inline void *peer_content(mac_check *state) {
        return state ? reinterpret_cast<void *>(state->button) : nullptr;
    }

    inline void *peer_content(mac_radio *state) {
        return state ? reinterpret_cast<void *>(state->button) : nullptr;
    }

    inline void *peer_content(mac_list *state) {
        return state ? reinterpret_cast<void *>(state->scroll) : nullptr;
    }

    inline void *peer_content(mac_combo_box *state) {
        return state ? reinterpret_cast<void *>(state->combo) : nullptr;
    }

    inline void *peer_content(mac_accordion *state) {
        return state ? reinterpret_cast<void *>(state->scroll) : nullptr;
    }

    inline void *peer_content(mac_tab_view *state) {
        return state ? reinterpret_cast<void *>(state->view) : nullptr;
    }

    inline void *peer_content(mac_split_view *state) {
        return state ? reinterpret_cast<void *>(state->view) : nullptr;
    }

    inline void *peer_content(mac_icon_view *state) {
        return state ? reinterpret_cast<void *>(state->scroll) : nullptr;
    }

    inline void *peer_content(mac_tree_view *state) {
        return state ? reinterpret_cast<void *>(state->scroll) : nullptr;
    }

    inline void *peer_content(mac_table_view *state) {
        return state ? reinterpret_cast<void *>(state->scroll) : nullptr;
    }

    inline void *peer_content(mac_code_edit *state) {
        return state ? reinterpret_cast<void *>(state->view) : nullptr;
    }

    inline void *peer_content(mac_surface *state) {
        return state ? reinterpret_cast<void *>(state->view) : nullptr;
    }

    inline constexpr native::detail::peer_bindings<
        native::panel *, mac_surface *> panel_bindings;
    inline constexpr native::detail::peer_bindings<
        native::canvas *, mac_surface *> canvas_bindings;
    inline constexpr native::detail::peer_bindings<
        native::check *, mac_check *> check_bindings;
    inline constexpr native::detail::peer_bindings<
        native::radio *, mac_radio *> radio_bindings;
    inline constexpr native::detail::peer_bindings<
        native::list *, mac_list *> list_bindings;
    inline constexpr native::detail::peer_bindings<
        native::combo_box *, mac_combo_box *> combo_box_bindings;
    inline constexpr native::detail::peer_bindings<
        native::accordion *, mac_accordion *> accordion_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tab_view *, mac_tab_view *> tab_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::split_view *, mac_split_view *> split_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::icon_view *, mac_icon_view *> icon_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::tree_view *, mac_tree_view *> tree_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::table_view *, mac_table_view *> table_view_bindings;
    inline constexpr native::detail::peer_bindings<
        native::code_edit *, mac_code_edit *> code_edit_bindings;
    extern native::bindings<native::file_dialog *, NSSavePanel *>
        file_dialog_bindings;

    // Return the outer NSView used by any public child control.
    NSView *view_from_control(native::wnd *control);

    // Return the child-content view of a created window or control.
    NSView *parent_view(native::wnd *parent,
                        native::wnd *child = nullptr);
} // namespace mac
