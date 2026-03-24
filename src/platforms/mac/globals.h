#pragma once

#import <Cocoa/Cocoa.h>
#include <native.h>
#include <bindings.h>

namespace mac
{
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

    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, macgpx *> wnd_gpx_bindings;
}
