#include <iostream>
#import <AppKit/AppKit.h>
#include <native.h>
#include "globals.h"

namespace native
{

    void app_wnd::create() const
    {
        NSRect frame = NSMakeRect(x(), y(), width(), height());
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

        NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:style
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];

        [window setTitle:[NSString stringWithUTF8String:title().c_str()]];
        gnustep::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
    }

    void app_wnd::show() const
    {
        NSWindow *win = gnustep::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!win)
        {
            std::cerr << "GNUstep: Can't show window, not created.\n";
            return;
        }

        [win makeKeyAndOrderFront:nil];
    }

} // namespace native
