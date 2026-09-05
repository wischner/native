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
    native::bindings<uint32_t, mac_font *> font_bindings;
    native::bindings<uint32_t, mac_menu *> menu_bindings;
    // Bind: structural panel host to its container view.
    // Bind: paintable canvas to its drawing view.
    native::bindings<native::file_dialog *, NSSavePanel *>
        file_dialog_bindings;

    NSView *view_from_control(native::wnd *control) {
        return control
                   ? static_cast<NSView *>(
                         native::detail::wnd_peer_access::content(*control))
                   : nil;
    }

    NSView *parent_view(native::wnd *parent, native::wnd *child) {
        if (!parent || !parent->get_created())
            return nil;
        if (auto *accordion = dynamic_cast<native::accordion *>(parent)) {
            auto *binding = accordion_bindings.object_from_handle(accordion);
            return binding ? binding->stack : nil;
        }
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *binding = tab_view_bindings.object_from_handle(tabs);
            if (!binding) return nil;
            for (std::size_t index = 0; index < tabs->get_item_count(); ++index) {
                if (&tabs->get_item(index).get_content() == child &&
                    index < static_cast<std::size_t>([binding->view numberOfTabViewItems]))
                    return [[binding->view tabViewItemAtIndex:index] view];
            }
            return nil;
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
