//
// Implements the macOS native-window bridge backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include "globals.h"
#include <native.h>
#include <bindings.h>
#include "native_window.h"

using namespace native;

@interface NativeWindowDelegate : NSObject <NSWindowDelegate>
{
    native::app_wnd *_owner;
}

- (id)initWithOwner:(native::app_wnd *)owner;
@end

@implementation NativeWindowDelegate

- (id)initWithOwner:(native::app_wnd *)owner {
    self = [super init];
    if (self)
        _owner = owner;
    return self;
}

- (void)windowDidMove:(NSNotification *)notification {
    if (!_owner)
        return;

    NSRect frame = [[notification object] frame];
    native::point position(
        static_cast<native::coord>(frame.origin.x),
        static_cast<native::coord>(frame.origin.y));
    _owner->on_native_move(position);
    _owner->on_wnd_move.emit(position);
}

- (void)windowDidResize:(NSNotification *)notification {
    if (!_owner)
        return;

    NSRect bounds = [[[notification object] contentView] bounds];
    native::size dimensions(
        static_cast<native::dim>(bounds.size.width),
        static_cast<native::dim>(bounds.size.height));
    _owner->on_native_resize(dimensions);
    _owner->on_wnd_resize.emit(dimensions);
}

- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    [mac::global_app stop:nil];
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    NSWindow *window = [notification object];
    native::app_wnd *owner = _owner;
    _owner = nullptr;

    if (owner) {
        owner->on_native_destroy();
        mac::delegate_bindings.unregister_by_handle(owner);
    }
    mac::wnd_bindings.unregister_by_handle(window);
    [window setDelegate:nil];

    // Defer releases until AppKit finishes the close notification.
    [window autorelease];
    [self autorelease];
}
@end

namespace mac {

native_window::native_window(
    app_wnd *owner,
    const char *title,
    int x,
    int y,
    int width,
    int height) {
    NSRect frame = NSMakeRect(x, y, width, height);
    auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
    [win setReleasedWhenClosed:NO];

    NSString *ns_title = [NSString stringWithUTF8String:title];
    [win setTitle:ns_title];

    auto delegate =
        [[NativeWindowDelegate alloc] initWithOwner:owner];
    [win setDelegate:delegate];

    mac::wnd_bindings.register_pair(win, owner);
    mac::delegate_bindings.register_pair(owner, delegate);
}

} // namespace mac
