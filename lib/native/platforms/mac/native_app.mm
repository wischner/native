//
// Implements the macOS application-bootstrap backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <native.h>
#include "globals.h"

__attribute__((constructor)) static void init_mac_app() {
    mac::global_app = [NSApplication sharedApplication];
    [mac::global_app
        setActivationPolicy:NSApplicationActivationPolicyRegular];

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                          handler:^NSEvent *(
                                              NSEvent *event) {
        NSWindow *window = [event window];
        native::wnd *target =
            window ? mac::wnd_bindings.object_from_handle(window)
                   : nullptr;
        return target && !target->get_input_enabled() ? nil : event;
    }];
    [mac::global_app finishLaunching];
}
