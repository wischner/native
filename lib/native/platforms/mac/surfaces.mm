//
// Implements the macOS structural container and paintable child
// surface. Both are plain child NSViews, so AppKit supplies the
// hierarchy every other Native control already parents itself into.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <stdexcept>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "globals.h"

// A structural host draws no content of its own beyond the standard
// container background, and never becomes first responder merely by
// existing.
@interface native_panel_view : NSView {
@public
    native::panel *_owner;
}
@end

@implementation native_panel_view
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return NO; }

- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    // Exposed panel space shows the ordinary control-host surface
    // rather than whatever the window last left there.
    [[NSColor windowBackgroundColor] set];
    NSRectFill(dirty);
}

- (native::point)localPoint:(NSEvent *)event {
    const NSPoint value = [self convertPoint:[event locationInWindow]
                                    fromView:nil];
    return native::point(static_cast<native::coord>(value.x),
                         static_cast<native::coord>(value.y));
}

- (void)mouseDown:(NSEvent *)event {
    if (_owner) {
        _owner->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            native::mouse_action::press,
            [self localPoint:event]));
    }
}

- (void)mouseUp:(NSEvent *)event {
    if (_owner) {
        _owner->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            native::mouse_action::release,
            [self localPoint:event]));
    }
}

- (void)mouseMoved:(NSEvent *)event {
    if (_owner)
        _owner->on_native_mouse_move([self localPoint:event]);
}

- (void)scrollWheel:(NSEvent *)event {
    if (_owner) {
        _owner->on_native_mouse_wheel(native::mouse_wheel_event(
            [self localPoint:event],
            static_cast<native::coord>([event scrollingDeltaY]),
            native::wheel_direction::vertical));
    }
}
@end

// The application owns every client pixel of a canvas, so this view
// adds no background of its own.
@interface native_canvas_view : NSView {
@public
    native::canvas *_owner;
}
@end

@implementation native_canvas_view
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    if (!_owner || !_owner->get_created())
        return;
    native::rect invalid(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(dirty.size.width),
        static_cast<native::dim>(dirty.size.height));
    native::gpx &graphics = _owner->get_gpx();
    graphics.set_clip(invalid);
    _owner->on_native_paint(
        native::wnd_paint_event(invalid, graphics));
}

- (native::point)localPoint:(NSEvent *)event {
    const NSPoint value = [self convertPoint:[event locationInWindow]
                                    fromView:nil];
    return native::point(static_cast<native::coord>(value.x),
                         static_cast<native::coord>(value.y));
}

- (void)mouseDown:(NSEvent *)event {
    [[self window] makeFirstResponder:self];
    if (_owner) {
        _owner->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            native::mouse_action::press,
            [self localPoint:event]));
    }
}

- (void)mouseUp:(NSEvent *)event {
    if (_owner) {
        _owner->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            native::mouse_action::release,
            [self localPoint:event]));
    }
}

- (void)mouseMoved:(NSEvent *)event {
    if (_owner)
        _owner->on_native_mouse_move([self localPoint:event]);
}

- (void)mouseDragged:(NSEvent *)event {
    if (_owner)
        _owner->on_native_mouse_move([self localPoint:event]);
}

- (void)scrollWheel:(NSEvent *)event {
    if (_owner) {
        _owner->on_native_mouse_wheel(native::mouse_wheel_event(
            [self localPoint:event],
            static_cast<native::coord>([event scrollingDeltaY]),
            native::wheel_direction::vertical));
    }
}
@end

namespace native
{
    void panel::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: panel requires a created parent.");

        native_panel_view *view = [[native_panel_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        view->_owner = self;
        [view setHidden:YES];
        [parent addSubview:view];

        auto *binding = new mac::mac_surface();
        binding->view = view;

        // Children resolve their parent view through this registry, so
        // the binding has to exist before on_wnd_create runs.
        mac::panel_bindings.register_pair(self, binding);
    }

    void panel::show_native() {
        auto *binding = mac::panel_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("macOS: panel is not created.");
        [binding->view setHidden:NO];
    }

    void panel::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *binding = mac::panel_bindings.object_from_handle(self);
        if (binding) {
            [binding->view removeFromSuperview];
            [binding->view release];
            mac::panel_bindings.unregister_by_handle(self);
            delete binding;
        }
    }

    void canvas::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: canvas requires a created parent.");

        native_canvas_view *view = [[native_canvas_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        view->_owner = self;
        [view setHidden:YES];
        [parent addSubview:view];

        auto *binding = new mac::mac_surface();
        binding->view = view;
        mac::canvas_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        self->relayout_children();
    }

    void canvas::show_native() {
        auto *binding = mac::canvas_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("macOS: canvas is not created.");
        [binding->view setHidden:NO];
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *binding = mac::canvas_bindings.object_from_handle(self);
        if (binding) {
            [binding->view removeFromSuperview];
            [binding->view release];
            mac::canvas_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
