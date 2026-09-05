//
// Implements the macOS window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <native.h>
#include <native/wnd.h>
#include <bindings.h>
#include <AppKit/AppKit.h>
#include <objc/runtime.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    char cursor_area_key;
    char cursor_target_key;

    // AppKit containers allocate page/pane bounds. Do not replace those
    // with the shared fallback's approximation of native edge metrics.
    NSRect content_frame(native::wnd &owner, NSView *view, NSRect proposed) {
        auto *parent = owner.get_parent();
        if ([view superview] &&
            (dynamic_cast<native::tab_view *>(parent) ||
             dynamic_cast<native::split_view *>(parent))) {
            const NSSize size = [[view superview] bounds].size;
            const native::size actual(
                static_cast<native::dim>(std::clamp<CGFloat>(size.width, 0, 65535)),
                static_cast<native::dim>(std::clamp<CGFloat>(size.height, 0, 65535)));
            if (owner.get_position().x || owner.get_position().y)
                owner.on_native_move(native::point(0, 0));
            if (owner.get_dimensions().w != actual.w ||
                owner.get_dimensions().h != actual.h)
                owner.on_native_resize(actual);
            return NSMakeRect(0, 0, actual.w, actual.h);
        }
        return proposed;
    }

    NSCursor *cursor_for(native::mouse_cursor cursor) {
        if (cursor == native::mouse_cursor::ibeam)
            return [NSCursor IBeamCursor];
        if (cursor == native::mouse_cursor::crosshair)
            return [NSCursor crosshairCursor];
        if (cursor == native::mouse_cursor::resize_horizontal)
            return [NSCursor resizeLeftRightCursor];
        if (cursor == native::mouse_cursor::resize_vertical)
            return [NSCursor resizeUpDownCursor];
        if (cursor == native::mouse_cursor::resize_northwest_southeast ||
            cursor == native::mouse_cursor::resize_northeast_southwest)
            return [NSCursor crosshairCursor];
        return [NSCursor arrowCursor];
    }
} // namespace

@interface native_cursor_target : NSObject {
    NSCursor *_cursor;
}

- (id)initWithCursor:(NSCursor *)cursor;
- (void)cursorUpdate:(NSEvent *)event;
@end

@implementation native_cursor_target

- (id)initWithCursor:(NSCursor *)cursor {
    self = [super init];
    if (self)
        _cursor = [cursor retain];
    return self;
}

- (void)dealloc {
    [_cursor release];
    [super dealloc];
}

- (void)cursorUpdate:(NSEvent *)event {
    (void)event;
    [_cursor set];
}

@end

namespace native
{
    void wnd::apply_position() {
        if (NSView *control = mac::view_from_control(this)) {
            NSRect frame = [control frame];
            frame.origin = NSMakePoint(_bounds.p.x, _bounds.p.y);
            [control setFrame:content_frame(*this, control, frame)];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window
                setFrameOrigin:NSMakePoint(_bounds.p.x, _bounds.p.y)];
        }
    }

    void wnd::apply_dimensions() {
        if (NSView *control = mac::view_from_control(this)) {
            NSRect frame = [control frame];
            frame.size = NSMakeSize(_bounds.d.w, _bounds.d.h);
            [control setFrame:content_frame(*this, control, frame)];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window
                setContentSize:NSMakeSize(_bounds.d.w, _bounds.d.h)];
        }
    }

    void wnd::apply_bounds() {
        NSRect frame = NSMakeRect(
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h);

        if (NSView *control = mac::view_from_control(this)) {
            [control setFrame:content_frame(*this, control, frame)];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window)
            [window setFrame:frame display:YES];
    }

    void wnd::apply_parent() {
        if (NSView *control = mac::view_from_control(this)) {
            [control removeFromSuperview];
            if (NSView *parent = mac::parent_view(_parent, this))
                [parent addSubview:control];
            return;
        }

        NSWindow *child = mac::wnd_bindings.handle_from_object(this);
        if (!child)
            return;

        if ([child parentWindow])
            [[child parentWindow] removeChildWindow:child];

        NSWindow *parent =
            _parent ? mac::wnd_bindings.handle_from_object(_parent)
                    : nil;
        if (parent)
            [parent addChildWindow:child ordered:NSWindowAbove];
    }

    void wnd::apply_cursor() {
        NSView *view = mac::view_from_control(this);
        if (!view) {
            NSWindow *window =
                mac::wnd_bindings.handle_from_object(this);
            view = window ? [window contentView] : nil;
        }
        if (!view)
            return;

        NSTrackingArea *old_area = objc_getAssociatedObject(
            view, &cursor_area_key);
        if (old_area)
            [view removeTrackingArea:old_area];

        NSCursor *cursor = cursor_for(_cursor);
        native_cursor_target *target =
            [[native_cursor_target alloc] initWithCursor:cursor];
        NSTrackingArea *area = [[NSTrackingArea alloc]
            initWithRect:NSZeroRect
                 options:NSTrackingCursorUpdate |
                         NSTrackingInVisibleRect |
                         NSTrackingActiveAlways
                   owner:target
                userInfo:nil];
        [view addTrackingArea:area];
        objc_setAssociatedObject(view,
                                 &cursor_target_key,
                                 target,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        objc_setAssociatedObject(view,
                                 &cursor_area_key,
                                 area,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [target release];
        [area release];

        NSWindow *window = [view window];
        if (window) {
            NSPoint point = [view convertPoint:
                [window mouseLocationOutsideOfEventStream]
                                      fromView:nil];
            if (NSMouseInRect(point, [view bounds], [view isFlipped]))
                [cursor set];
        }
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;

        if (NSView *control =
                mac::view_from_control(this)) {
            [control setNeedsDisplay:YES];
            return *this;
        }

        NSWindow *nswin = mac::wnd_bindings.handle_from_object(
            this);
        if (nswin) {
            [[nswin contentView] setNeedsDisplay:YES];
        }

        return *this;
    }

    wnd &wnd::invalidate_native(const rect &r) {
        if (!_created)
            return *this;

        if (NSView *control =
                mac::view_from_control(this)) {
            [control
                setNeedsDisplayInRect:NSMakeRect(
                                          r.p.x, r.p.y, r.d.w, r.d.h)];
            return *this;
        }

        NSWindow *nswin = mac::wnd_bindings.handle_from_object(
            this);
        if (nswin) {
            NSRect rect = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
            [[nswin contentView] setNeedsDisplayInRect:rect];
        }

        return *this;
    }

    gpx &wnd::get_gpx() {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
