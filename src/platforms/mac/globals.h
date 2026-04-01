#pragma once

#import <Cocoa/Cocoa.h>
#include <native.h>
#include <bindings.h>

namespace mac
{
    // Platform handle for a font_t — retains an NSFont.
    struct macfont
    {
        NSFont *ns_font;
    };

    // Graphics cache structure for macOS Cocoa/Quartz
    struct macgpx
    {
        NSView *view = nullptr;           // Cached NSView for drawing
        NSGraphicsContext *context = nullptr; // Cached graphics context

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    struct macmenu {
        NSMenu *ns_menu = nil;
        native::app_wnd *owner = nullptr;
    };

    struct macbutton {
        NSButton *ns_button = nil;
        id target = nil;
        native::button *owner = nullptr;
    };

    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, macgpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, macfont *> font_bindings;
    extern native::bindings<uint32_t, macmenu *> menu_bindings;
    extern native::bindings<native::button *, macbutton *> button_bindings;
}
