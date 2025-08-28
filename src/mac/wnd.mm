#include <native.h>
#include <bindings.h>
#include <AppKit/AppKit.h>
#include <iostream>
#include "globals.h"

namespace mac
{
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
}

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

        if (!window)
        {
            std::cerr << "macOS: Failed to create NSWindow.\n";
            return;
        }

        [window setTitle:[NSString stringWithUTF8String:title().c_str()]];
        mac::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
    }

    void app_wnd::show() const
    {
        NSWindow *window = mac::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!window)
        {
            std::cerr << "macOS: Cannot show window — not created.\n";
            return;
        }

        [window makeKeyAndOrderFront:nil];
    }

} // namespace native
