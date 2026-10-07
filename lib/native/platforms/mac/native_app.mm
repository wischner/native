//
// Implements the macOS application-bootstrap backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <native.h>
#include "globals.h"
#include "../../input_state.h"
#include "keyboard.h"

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
        if (target && ([event type] == NSEventTypeKeyDown ||
                       [event type] == NSEventTypeKeyUp)) {
            auto physical = mac::physical_key(event, [event type] == NSEventTypeKeyDown
                ? native::key_action::press : native::key_action::release);
            if (native::detail::key_held(*target, physical.key)) {
                target->on_native_key(physical);
                return nil;
            }
        }
        if (target && [event type] == NSEventTypeLeftMouseDown)
            native::detail::reset_input(*target);
        return target && !target->get_input_enabled() ? nil : event;
    }];
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSMenuDidBeginTrackingNotification object:nil
        queue:nil usingBlock:^(NSNotification *) {
            NSWindow *window = [mac::global_app keyWindow];
            if (auto *owner = mac::wnd_bindings.object_from_handle(window))
                native::detail::reset_input(*owner);
        }];
    [mac::global_app finishLaunching];
}
