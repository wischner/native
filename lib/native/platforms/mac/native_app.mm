//
// Implements the macOS application-bootstrap backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include "globals.h"

__attribute__((constructor))
static void init_mac_app() {
    mac::global_app = [NSApplication sharedApplication];
    [mac::global_app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [mac::global_app finishLaunching];
}
