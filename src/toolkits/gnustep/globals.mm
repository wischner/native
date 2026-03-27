#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <native.h>

#include "globals.h"

extern "C" void GSInitializeProcess(int argc, char **argv, char **envp);

namespace gnustep
{
    NSApplication *global_app = nil;
    NSAutoreleasePool *process_pool = nil;
    bool app_initialized = false;
    bool exit_requested = false;
    bool handling_window_close = false;

    native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    native::bindings<NSView *, native::wnd *> view_bindings;
    native::bindings<native::wnd *, gnustepgpx *> wnd_gpx_bindings;
    native::bindings<native::wnd *, id> delegate_bindings;
    native::bindings<uint32_t, gnustepfont *> font_bindings;
    native::bindings<uint32_t, gnustepmenu *> menu_bindings;

    void ensure_app_initialized()
    {
        if (app_initialized)
            return;

        if (!process_pool)
            process_pool = [[NSAutoreleasePool alloc] init];

        GSInitializeProcess(native::app::argc, native::app::argv, native::app::envp);

        global_app = [NSApplication sharedApplication];
        if (!global_app)
            return;

        app_initialized = true;
    }

    void request_main_loop_exit()
    {
        exit_requested = true;

        if (!global_app)
            return;

        [global_app stop:nil];

        NSEvent *wake = [NSEvent otherEventWithType:NSApplicationDefined
                                           location:NSMakePoint(0, 0)
                                      modifierFlags:0
                                          timestamp:0
                                       windowNumber:0
                                            context:nil
                                            subtype:0
                                              data1:0
                                              data2:0];
        [global_app postEvent:wake atStart:NO];
    }
}
