#pragma once

#import <Cocoa/Cocoa.h>
#include <native.h>
#include <bindings.h>

namespace mac
{
    extern NSApplication *global_app;
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
}
