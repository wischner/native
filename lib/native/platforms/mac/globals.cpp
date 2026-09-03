//
// Implements the macOS shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"
#include <native.h>
#include <bindings.h>
#import <Cocoa/Cocoa.h>

namespace mac
{
    NSApplication *global_app = nullptr;
    native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, id> delegate_bindings;
    native::bindings<native::wnd *, mac_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, mac_font *> font_bindings;
    native::bindings<uint32_t, mac_menu *> menu_bindings;
    native::bindings<native::button *, mac_button *> button_bindings;
    native::bindings<native::text_edit *, mac_text_edit *>
        text_edit_bindings;
    // Bind: structural panel host to its container view.
    native::bindings<native::panel *, mac_surface *> panel_bindings;
    // Bind: paintable canvas to its drawing view.
    native::bindings<native::canvas *, mac_surface *> canvas_bindings;
    native::bindings<native::check *, mac_check *> check_bindings;
    native::bindings<native::radio *, mac_radio *> radio_bindings;
    native::bindings<native::list *, mac_list *> list_bindings;
    native::bindings<native::combo_box *, mac_combo_box *>
        combo_box_bindings;
    native::bindings<native::accordion *, mac_accordion *>
        accordion_bindings;
    native::bindings<native::tab_view *, mac_tab_view *>
        tab_view_bindings;
    native::bindings<native::split_view *, mac_split_view *>
        split_view_bindings;
    native::bindings<native::icon_view *, mac_icon_view *>
        icon_view_bindings;
    native::bindings<native::tree_view *, mac_tree_view *>
        tree_view_bindings;
    native::bindings<native::table_view *, mac_table_view *>
        table_view_bindings;
    native::bindings<native::code_edit *, mac_code_edit *>
        code_edit_bindings;
    native::bindings<native::file_dialog *, NSSavePanel *>
        file_dialog_bindings;

    NSView *view_from_control(native::wnd *control) {
        if (auto *host = dynamic_cast<native::panel *>(control)) {
            auto *binding = panel_bindings.object_from_handle(host);
            return binding ? binding->view : nil;
        }
        if (auto *surface = dynamic_cast<native::canvas *>(control)) {
            auto *binding = canvas_bindings.object_from_handle(surface);
            return binding ? binding->view : nil;
        }
        if (auto *button = dynamic_cast<native::button *>(control)) {
            auto *binding = button_bindings.object_from_handle(button);
            return binding ? binding->ns_button : nil;
        }
        if (auto *check = dynamic_cast<native::check *>(control)) {
            auto *binding = check_bindings.object_from_handle(check);
            return binding ? binding->button : nil;
        }
        if (auto *radio = dynamic_cast<native::radio *>(control)) {
            auto *binding = radio_bindings.object_from_handle(radio);
            return binding ? binding->button : nil;
        }
        if (auto *list = dynamic_cast<native::list *>(control)) {
            auto *binding = list_bindings.object_from_handle(list);
            return binding ? binding->scroll : nil;
        }
        if (auto *accordion =
                dynamic_cast<native::accordion *>(control)) {
            auto *binding =
                accordion_bindings.object_from_handle(accordion);
            return binding ? binding->stack : nil;
        }
        if (auto *tabs = dynamic_cast<native::tab_view *>(control)) {
            auto *binding = tab_view_bindings.object_from_handle(tabs);
            return binding ? binding->view : nil;
        }
        if (auto *split = dynamic_cast<native::split_view *>(control)) {
            auto *binding = split_view_bindings.object_from_handle(split);
            return binding ? binding->view : nil;
        }
        if (auto *icons = dynamic_cast<native::icon_view *>(control)) {
            auto *binding = icon_view_bindings.object_from_handle(icons);
            return binding ? binding->scroll : nil;
        }
        if (auto *tree = dynamic_cast<native::tree_view *>(control)) {
            auto *binding = tree_view_bindings.object_from_handle(tree);
            return binding ? binding->scroll : nil;
        }
        if (auto *table = dynamic_cast<native::table_view *>(control)) {
            auto *binding = table_view_bindings.object_from_handle(table);
            return binding ? binding->scroll : nil;
        }
        if (auto *editor = dynamic_cast<native::code_edit *>(control)) {
            auto *binding = code_edit_bindings.object_from_handle(editor);
            return binding ? binding->view : nil;
        }
        if (auto *editor =
                dynamic_cast<native::text_edit *>(control)) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (!binding)
                return nil;
            return binding->scroll
                       ? static_cast<NSView *>(binding->scroll)
                       : static_cast<NSView *>(binding->field);
        }
        return nil;
    }

    NSView *parent_view(native::wnd *parent, native::wnd *child) {
        if (!parent || !parent->get_created())
            return nil;
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *binding = tab_view_bindings.object_from_handle(tabs);
            return binding ? binding->page_host : nil;
        }
        if (auto *split = dynamic_cast<native::split_view *>(parent)) {
            auto *binding = split_view_bindings.object_from_handle(split);
            if (!binding) return nil;
            return child == &split->get_second()
                       ? binding->second
                       : binding->first;
        }
        if (NSView *view = view_from_control(parent))
            return view;
        NSWindow *window = wnd_bindings.handle_from_object(parent);
        return window ? [window contentView] : nil;
    }
} // namespace mac
