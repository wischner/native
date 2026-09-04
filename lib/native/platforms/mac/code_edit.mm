//
// Implements a Cocoa child view for the shared painted source editor.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <stdexcept>
#include <string>

#include <native.h>

#include "globals.h"

@interface native_code_edit_view : NSView {
@public
    native::code_edit *_owner;
}
@end

@implementation native_code_edit_view
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

- (BOOL)becomeFirstResponder {
    const BOOL result = [super becomeFirstResponder];
    if (result && _owner)
        _owner->on_native_focus(true);
    return result;
}

- (BOOL)resignFirstResponder {
    const BOOL result = [super resignFirstResponder];
    if (result && _owner)
        _owner->on_native_focus(false);
    return result;
}

- (native::point)localPoint:(NSEvent *)event {
    const NSPoint value = [self convertPoint:[event locationInWindow]
                                    fromView:nil];
    return native::point(
        static_cast<native::coord>(value.x),
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

- (void)scrollWheel:(NSEvent *)event {
    if (!_owner)
        return;
    _owner->on_native_mouse_wheel(native::mouse_wheel_event(
        [self localPoint:event],
        static_cast<native::coord>([event scrollingDeltaY]),
        native::wheel_direction::vertical));
}

- (void)keyDown:(NSEvent *)event {
    if (!_owner)
        return;
    const NSEventModifierFlags modifiers = [event modifierFlags];
    const bool extend =
        (modifiers & NSEventModifierFlagShift) != 0;
    const bool command =
        (modifiers & NSEventModifierFlagCommand) != 0;
    NSString *plain = [event charactersIgnoringModifiers];
    if (command && [plain length] != 0) {
        const unichar value = [plain characterAtIndex:0];
        native::code_edit_key key;
        bool handled = true;
        if (value == 'a' || value == 'A')
            key = native::code_edit_key::select_all;
        else if (value == 'c' || value == 'C')
            key = native::code_edit_key::copy;
        else if (value == 'x' || value == 'X')
            key = native::code_edit_key::cut;
        else if (value == 'v' || value == 'V')
            key = native::code_edit_key::paste;
        else if (value == 'z' || value == 'Z')
            key = extend ? native::code_edit_key::redo
                         : native::code_edit_key::undo;
        else
            handled = false;
        if (handled)
            _owner->on_native_key(key);
        else
            [super keyDown:event];
        return;
    }

    native::code_edit_key key;
    bool handled = true;
    switch ([event keyCode]) {
    case 123:
        key = native::code_edit_key::left;
        break;
    case 124:
        key = native::code_edit_key::right;
        break;
    case 125:
        key = native::code_edit_key::down;
        break;
    case 126:
        key = native::code_edit_key::up;
        break;
    case 115:
        key = native::code_edit_key::home;
        break;
    case 119:
        key = native::code_edit_key::end;
        break;
    case 116:
        key = native::code_edit_key::page_up;
        break;
    case 121:
        key = native::code_edit_key::page_down;
        break;
    case 51:
        key = native::code_edit_key::backspace;
        break;
    case 117:
        key = native::code_edit_key::delete_forward;
        break;
    case 36:
    case 76:
        key = native::code_edit_key::enter;
        break;
    case 48:
        key = native::code_edit_key::tab;
        break;
    case 53:
        key = native::code_edit_key::escape;
        break;
    default:
        handled = false;
        break;
    }
    if (handled) {
        _owner->on_native_key(key, extend);
        return;
    }
    NSString *characters = [event characters];
    const char *utf8 = [characters UTF8String];
    if (utf8 && *utf8)
        _owner->on_native_text_input(std::string(utf8));
    else
        [super keyDown:event];
}
@end

namespace native
{
    void code_edit::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: code_edit requires a created parent.");
        native_code_edit_view *view = [[native_code_edit_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        view->_owner = self;
        [parent addSubview:view];
        auto *binding = new mac::mac_code_edit();
        binding->view = view;
        mac::code_edit_bindings.register_pair(self, binding);
        self->invalidate();
    }

    void code_edit::show_native() {
        auto *binding = mac::code_edit_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error(
                "macOS: code_edit is not created.");
        [binding->view setHidden:NO];
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            mac::code_edit_bindings.object_from_handle(self);
        if (binding) {
            [binding->view removeFromSuperview];
            [binding->view release];
            mac::code_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
