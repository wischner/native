#include "globals.h"
#include <native.h>
#include <bindings.h>
#import <Cocoa/Cocoa.h>

namespace mac
{
    NSApplication *global_app = nullptr;
    native::bindings<NSWindow *, native::wnd *> wnd_bindings;
}
