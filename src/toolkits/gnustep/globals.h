#pragma once

#import <AppKit/AppKit.h>
#include <native.h>
#include <bindings.h>

namespace gnustep
{
    // Graphics cache structure for GNUstep (same as macOS)
    typedef struct
    {
        NSView *view = nullptr;           // Cached NSView for drawing
        NSGraphicsContext *context = nullptr; // Cached graphics context

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    } gnustepgpx;

    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, gnustepgpx *> wnd_gpx_bindings;
}
