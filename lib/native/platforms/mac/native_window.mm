//
// Implements the macOS native-window bridge backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "native_window.h"

#include <native.h>
#include <bindings.h>

#include "globals.h"

// Translate AppKit window lifecycle events to a borrowed app_wnd.
@interface native_window_delegate : NSObject <NSWindowDelegate> {
    native::app_wnd *_owner;
}

- (id)init_with_owner:(native::app_wnd *)owner;
@end

@implementation native_window_delegate

- (id)init_with_owner:(native::app_wnd *)owner {
    self = [super init];
    if (self)
        _owner = owner;
    return self;
}

- (void)windowDidMove:(NSNotification *)notification {
    if (!_owner)
        return;

    NSRect frame = [[notification object] frame];
    native::point position(static_cast<native::coord>(frame.origin.x),
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
    NSWindow *window = static_cast<NSWindow *>(sender);
    if ([window sheetParent])
        [[window sheetParent] endSheet:window];
    if (_owner)
        _owner->on_native_destroy();
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    NSWindow *window = [notification object];
    native::app_wnd *owner = _owner;
    _owner = nullptr;

    if (owner) {
        if (owner->get_created())
            owner->on_native_destroy();
        mac::delegate_bindings.unregister_by_handle(owner);
        if (owner == native::app::main_wnd() && mac::global_app)
            [mac::global_app stop:nil];
        if (owner->get_modal()) {
            native::app_wnd *parent = owner->get_owner();
            native::app_wnd *focus =
                parent && parent->get_input_enabled()
                    ? parent
                    : parent ? parent->get_active_modal() : nullptr;
            NSWindow *focus_window =
                focus ? mac::wnd_bindings.handle_from_object(focus)
                      : nil;
            if (focus_window)
                [focus_window makeKeyAndOrderFront:nil];
        }
    }
    mac::wnd_bindings.unregister_by_handle(window);
    [window setDelegate:nil];

    // Defer releases until AppKit finishes the close notification.
    [window autorelease];
    [self autorelease];
}
@end

// Dispatch AppKit drawing through the portable window paint signal.
@interface native_content_view : NSView {
    native::app_wnd *_owner;
}
- (id)init_with_frame:(NSRect)frame owner:(native::app_wnd *)owner;
@end

@implementation native_content_view

- (id)init_with_frame:(NSRect)frame owner:(native::app_wnd *)owner {
    self = [super initWithFrame:frame];
    if (self)
        _owner = owner;
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (void)drawRect:(NSRect)dirty_rect {
    [super drawRect:dirty_rect];
    if (!_owner || !_owner->get_created())
        return;

    native::rect invalid(
        static_cast<native::coord>(dirty_rect.origin.x),
        static_cast<native::coord>(dirty_rect.origin.y),
        static_cast<native::dim>(dirty_rect.size.width),
        static_cast<native::dim>(dirty_rect.size.height));
    native::gpx &graphics = _owner->get_gpx();
    graphics.set_clip(invalid);
    native::wnd_paint_event event(invalid, graphics);
    _owner->on_wnd_paint.emit(event);
}

@end

namespace mac
{

    native_window::native_window(native::app_wnd *owner,
                                 const char *title,
                                 int x,
                                 int y,
                                 int width,
                                 int height) {
        NSRect frame = NSMakeRect(x, y, width, height);
        auto style = NSWindowStyleMaskTitled |
                     NSWindowStyleMaskClosable |
                     NSWindowStyleMaskResizable;
        NSWindow *win =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:style
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        [win setReleasedWhenClosed:NO];

        native_content_view *content =
            [[native_content_view alloc]
                init_with_frame:NSMakeRect(0, 0, width, height)
                          owner:owner];
        [win setContentView:content];
        [content release];

        NSString *ns_title = [NSString stringWithUTF8String:title];
        [win setTitle:ns_title];

        auto delegate =
            [[native_window_delegate alloc] init_with_owner:owner];
        [win setDelegate:delegate];

        mac::wnd_bindings.register_pair(win, owner);
        mac::delegate_bindings.register_pair(owner, delegate);
    }

} // namespace mac
