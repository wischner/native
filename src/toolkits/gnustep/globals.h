#pragma once

#import <AppKit/AppKit.h>

#include <native.h>
#include <bindings.h>

namespace gnustep
{
    typedef struct
    {
        NSView *view = nil;

        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    } gnustepgpx;

    extern NSApplication *global_app;
    extern bool app_initialized;
    extern bool exit_requested;
    extern bool handling_window_close;

    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<NSView *, native::wnd *> view_bindings;
    extern native::bindings<native::wnd *, gnustepgpx *> wnd_gpx_bindings;
    extern native::bindings<native::wnd *, id> delegate_bindings;

    void ensure_app_initialized();
    void request_main_loop_exit();
}
