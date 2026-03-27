#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <cmath>
#include <stdexcept>

#include <native.h>

#include "globals.h"

namespace
{
#if defined(NSWindowStyleMaskTitled)
    static const NSUInteger k_window_style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
#else
    static const NSUInteger k_window_style = NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask;
#endif

    native::mouse_button decode_button(NSInteger button)
    {
        switch (button)
        {
        case 0: return native::mouse_button::left;
        case 1: return native::mouse_button::right;
        case 2: return native::mouse_button::middle;
        default: return native::mouse_button::none;
        }
    }

    native::point to_native_point(NSView *view, NSEvent *event)
    {
        NSPoint p = [view convertPoint:[event locationInWindow] fromView:nil];
        return native::point(static_cast<native::coord>(p.x), static_cast<native::coord>(p.y));
    }

    void emit_move(NSView *view, void *owner_ptr, NSEvent *event)
    {
        native::app_wnd *owner = static_cast<native::app_wnd *>(owner_ptr);
        if (!owner)
            return;

        owner->on_mouse_move.emit(to_native_point(view, event));
    }

    void emit_click(NSView *view, void *owner_ptr, native::mouse_button button, native::mouse_action action, NSEvent *event)
    {
        native::app_wnd *owner = static_cast<native::app_wnd *>(owner_ptr);
        if (!owner || button == native::mouse_button::none)
            return;

        owner->on_mouse_click.emit(native::mouse_event(
            button,
            action,
            to_native_point(view, event)));
    }
}

@interface NativeView : NSView
{
@public
    void *_owner;
}

- (instancetype)initWithFrame:(NSRect)frame owner_ptr:(void *)owner_ptr;
@end

@implementation NativeView

- (instancetype)initWithFrame:(NSRect)frame owner_ptr:(void *)owner_ptr
{
    self = [super initWithFrame:frame];
    if (self)
        _owner = owner_ptr;
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    native::app_wnd *owner = static_cast<native::app_wnd *>(_owner);
    if (!owner)
        return;

    auto &g = owner->get_gpx();
    NSRect b = [self bounds];
    native::rect full(0, 0,
                      static_cast<native::dim>(b.size.width),
                      static_cast<native::dim>(b.size.height));

    g.set_clip(full);
    g.clear(g.paper());

    native::wnd_paint_event e(full, g);
    owner->on_wnd_paint.emit(e);
}

- (void)mouseMoved:(NSEvent *)event
{
    emit_move(self, _owner, event);
}

- (void)mouseDragged:(NSEvent *)event
{
    emit_move(self, _owner, event);
}

- (void)rightMouseDragged:(NSEvent *)event
{
    emit_move(self, _owner, event);
}

- (void)otherMouseDragged:(NSEvent *)event
{
    emit_move(self, _owner, event);
}

- (void)mouseDown:(NSEvent *)event
{
    // On GNUstep/Cocoa, mouseDown/mouseUp represent primary-button events.
    emit_click(self, _owner, native::mouse_button::left, native::mouse_action::press, event);
}

- (void)mouseUp:(NSEvent *)event
{
    emit_click(self, _owner, native::mouse_button::left, native::mouse_action::release, event);
}

- (void)rightMouseDown:(NSEvent *)event
{
    emit_click(self, _owner, native::mouse_button::right, native::mouse_action::press, event);
}

- (void)rightMouseUp:(NSEvent *)event
{
    emit_click(self, _owner, native::mouse_button::right, native::mouse_action::release, event);
}

- (void)otherMouseDown:(NSEvent *)event
{
    emit_click(self, _owner, decode_button([event buttonNumber]), native::mouse_action::press, event);
}

- (void)otherMouseUp:(NSEvent *)event
{
    emit_click(self, _owner, decode_button([event buttonNumber]), native::mouse_action::release, event);
}

- (void)scrollWheel:(NSEvent *)event
{
    native::app_wnd *owner = static_cast<native::app_wnd *>(_owner);
    if (!owner)
        return;

    CGFloat dx = [event deltaX];
    CGFloat dy = [event deltaY];

    native::wheel_direction dir = std::fabs(dy) >= std::fabs(dx)
        ? native::wheel_direction::vertical
        : native::wheel_direction::horizontal;

    CGFloat raw = dir == native::wheel_direction::vertical ? dy : dx;
    native::coord delta = 0;
    if (raw > 0)
        delta = static_cast<native::coord>(std::ceil(raw));
    else if (raw < 0)
        delta = static_cast<native::coord>(std::floor(raw));

    if (delta == 0 && raw != 0)
        delta = raw > 0 ? 1 : -1;

    if (delta == 0)
        return;

    owner->on_mouse_wheel.emit(native::mouse_wheel_event(
        to_native_point(self, event),
        delta,
        dir));
}

@end

@interface NativeWindowDelegate : NSObject <NSWindowDelegate>
{
@public
    void *_owner;
}

- (instancetype)initWithOwnerPtr:(void *)owner_ptr;
@end

@implementation NativeWindowDelegate

- (instancetype)initWithOwnerPtr:(void *)owner_ptr
{
    self = [super init];
    if (self)
        _owner = owner_ptr;
    return self;
}

- (void)windowDidResize:(NSNotification *)notification
{
    native::app_wnd *owner = static_cast<native::app_wnd *>(_owner);
    if (!owner)
        return;

    NSWindow *window = [notification object];
    NSRect b = [[window contentView] bounds];
    owner->on_wnd_resize.emit(native::size(
        static_cast<native::dim>(b.size.width),
        static_cast<native::dim>(b.size.height)));
}

- (void)windowDidMove:(NSNotification *)notification
{
    native::app_wnd *owner = static_cast<native::app_wnd *>(_owner);
    if (!owner)
        return;

    NSWindow *window = [notification object];
    NSRect frame = [window frame];
    owner->on_wnd_move.emit(native::point(
        static_cast<native::coord>(frame.origin.x),
        static_cast<native::coord>(frame.origin.y)));
}

- (void)windowWillClose:(NSNotification *)notification
{
    (void)notification;
    native::app_wnd *owner = static_cast<native::app_wnd *>(_owner);
    if (!owner)
        return;

    gnustep::handling_window_close = true;
    owner->destroy();
    gnustep::handling_window_close = false;
}

@end

namespace native
{

app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
    : wnd(x, y, w, h), _title(std::move(title))
{
}

app_wnd::app_wnd(const std::string &title, const point &pos, const size &dim)
    : app_wnd(title, pos.x, pos.y, dim.w, dim.h)
{
}

app_wnd::app_wnd(const std::string &title, const rect &bounds)
    : app_wnd(title, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
{
}

app_wnd &app_wnd::set_title(const std::string &title)
{
    _title = title;

    if (_created)
    {
        NSWindow *window = gnustep::wnd_bindings.from_b(this);
        if (window)
            [window setTitle:[NSString stringWithUTF8String:_title.c_str()]];
    }

    return *this;
}

const std::string &app_wnd::title() const
{
    return _title;
}

void app_wnd::create() const
{
    if (_created)
        return;

    gnustep::ensure_app_initialized();
    if (!gnustep::global_app)
        throw std::runtime_error("GNUstep: NSApplication initialization failed.");

    NSRect frame = NSMakeRect(_bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:k_window_style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    if (!window)
        throw std::runtime_error("GNUstep: Failed to create NSWindow.");

    if ([window respondsToSelector:@selector(setReleasedWhenClosed:)])
        [window setReleasedWhenClosed:NO];

    NativeView *view = [[NativeView alloc] initWithFrame:NSMakeRect(0, 0, _bounds.d.w, _bounds.d.h)
                                               owner_ptr:const_cast<app_wnd *>(this)];
    [window setContentView:view];
    [view release];

    NativeWindowDelegate *delegate = [[NativeWindowDelegate alloc] initWithOwnerPtr:const_cast<app_wnd *>(this)];
    [window setDelegate:delegate];

    [window setTitle:[NSString stringWithUTF8String:_title.c_str()]];
    [window setAcceptsMouseMovedEvents:YES];

    app_wnd *self = const_cast<app_wnd *>(this);
    gnustep::wnd_bindings.register_pair(window, self);
    gnustep::view_bindings.register_pair([window contentView], self);
    gnustep::delegate_bindings.register_pair(self, delegate);

    _created = true;
    self->menu.attach(*self);
    self->on_wnd_create.emit();
}

void app_wnd::show() const
{
    if (!_created)
        throw std::runtime_error("GNUstep: Cannot show window before it is created.");

    NSWindow *window = gnustep::wnd_bindings.from_b(const_cast<app_wnd *>(this));
    if (!window)
        throw std::runtime_error("GNUstep: Missing NSWindow binding for app_wnd.");

    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:[window contentView]];

    if (gnustep::global_app)
        [gnustep::global_app activateIgnoringOtherApps:YES];

    invalidate();
}

void app_wnd::destroy() const
{
    if (!_created)
        return;

    app_wnd *self = const_cast<app_wnd *>(this);
    _created = false;

    NSWindow *window = gnustep::wnd_bindings.from_b(self);
    NSView *view = gnustep::view_bindings.from_b(self);
    id delegate = gnustep::delegate_bindings.from_a(self);

    if (auto *cache = gnustep::wnd_gpx_bindings.from_a(self))
    {
        delete cache;
        gnustep::wnd_gpx_bindings.unregister_by_a(self);
    }

    if (view)
        gnustep::view_bindings.unregister_by_a(view);
    gnustep::wnd_bindings.unregister_by_b(self);
    gnustep::delegate_bindings.unregister_by_a(self);

    if (window)
    {
        [window setDelegate:nil];
        [window orderOut:nil];
        if (!gnustep::handling_window_close)
            [window close];
        [window release];
    }

    if (delegate)
        [delegate release];

    if (self == app::main_wnd())
        gnustep::request_main_loop_exit();
}

} // namespace native
