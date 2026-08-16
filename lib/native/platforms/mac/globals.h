//
// Declares internal macOS shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#import <Cocoa/Cocoa.h>
#include <native.h>
#include <bindings.h>

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

    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, id> delegate_bindings;
    extern native::bindings<native::wnd *, mac_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, mac_font *> font_bindings;
    extern native::bindings<uint32_t, mac_menu *> menu_bindings;
    extern native::bindings<native::button *, mac_button *>
        button_bindings;

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

    extern native::bindings<native::check *, mac_check *>
        check_bindings;
    extern native::bindings<native::radio *, mac_radio *>
        radio_bindings;
    extern native::bindings<native::list *, mac_list *> list_bindings;
    extern native::bindings<native::file_dialog *, NSSavePanel *>
        file_dialog_bindings;

    // Return the outer NSView used by any public child control.
    NSView *view_from_control(native::wnd *control);
} // namespace mac
