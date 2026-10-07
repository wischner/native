//
// Declares private AppKit physical position translation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#import <Cocoa/Cocoa.h>
#include <native/events.h>
namespace mac
{
    // Convert AppKit's physical virtual key and post-transition flags.
    native::key_event physical_key(NSEvent *event, native::key_action action);
}
