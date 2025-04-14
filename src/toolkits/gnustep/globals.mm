#include "globals.h"
#include <native.h>
#include <bindings.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

namespace gnustep
{
    NSApplication *global_app = nil;
    native::bindings<NSWindow *, native::wnd *> wnd_bindings;
}

// Ensure NSApplication is initialized on load
__attribute__((constructor))
static void init_gnustep_app()
{
    gnustep::global_app = [NSApplication sharedApplication];
}
