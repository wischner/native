//
// Implements the macOS application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>

#import <Cocoa/Cocoa.h>
#import <dispatch/dispatch.h>

#include "globals.h"
#include "../../post_backend.h"

namespace
{
    void drain_posted_work(void *) {
        @autoreleasepool {
            native::detail::drain_posted_work();

            // stop: takes effect when AppKit completes an event-loop
            // pass. Posted work can close the last window without an
            // NSEvent in flight, so supply a harmless event to finish
            // that pass instead of leaving run blocked indefinitely.
            if (mac::global_app) {
                NSEvent *wake = [NSEvent
                    otherEventWithType:NSEventTypeApplicationDefined
                    location:NSZeroPoint
                    modifierFlags:0
                    timestamp:0
                    windowNumber:0
                    context:nil
                    subtype:0
                    data1:0
                    data2:0];
                [mac::global_app postEvent:wake atStart:NO];
            }
        }
    }

    void wake_posted_work() {
        dispatch_async_f(dispatch_get_main_queue(),
                         nullptr,
                         drain_posted_work);
    }
} // namespace

namespace native
{

    int app::main_loop() {
        if (!mac::global_app)
            mac::global_app = [NSApplication sharedApplication];

        detail::set_loop_wake(wake_posted_work);
        wake_posted_work();
        [mac::global_app run];
        detail::set_loop_wake(nullptr);
        return 0;
    }

} // namespace native
