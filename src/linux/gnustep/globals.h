#pragma once

#import <AppKit/AppKit.h>
#include <native.h>
#include <bindings.h>

namespace gnustep
{
    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
}
