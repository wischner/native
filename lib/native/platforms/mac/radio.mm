//
// Implements the native AppKit radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#import <AppKit/AppKit.h>
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/radio.h>
#include "../../control_render_access.h"
#include "globals.h"

@interface native_radio_view : NSButton {
@public
    void *_nativeOwner;
}
@end

@implementation native_radio_view
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::radio *>(_nativeOwner);
    if (!owner || !owner->get_created()) {
        [super drawRect:dirty];
        return;
    }
    native::gpx &graphics = owner->get_gpx();
    auto appearance = native::theme::create(graphics);
    const NSRect frame = [self bounds];
    const native::rect bounds(
        0,
        0,
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.height)));
    graphics.set_clip(native::rect(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.height))));
    native::theme::state state;
    state.disabled = ![self isEnabled];
    state.focused = [[self window] firstResponder] == self;
    state.pressed = [self isHighlighted];
    native::detail::control_render_access::draw(
        *owner, graphics, *appearance, bounds, state);
}
@end
@interface native_radio_target : NSObject {
@public
    void *_owner;
}
- (void)action:(id)sender;
@end
@implementation native_radio_target
- (void)action:(id)sender {
    (void)sender;
    auto *o = static_cast<native::radio *>(_owner);
    if (o)
        o->on_native_selected();
}
@end
namespace
{
    NSString *text(const std::string &s) {
        NSString *v = [NSString stringWithUTF8String:s.c_str()];
        return v ? v : @"";
    }
    NSView *parent(native::radio *c) {
        auto *p = c->get_parent();
        NSView *view = mac::parent_view(p);
        if (!view)
            throw std::runtime_error(
                "macOS: radio requires a created parent.");
        return view;
    }
} // namespace
namespace native
{
    void radio::apply_text() {
        auto *b = mac::radio_bindings.object_from_handle(this);
        if (!b || !b->button)
            throw std::runtime_error("macOS: Missing radio binding.");
        [b->button setTitle:text(_text)];
    }
    void radio::apply_selected() {
        auto *b = mac::radio_bindings.object_from_handle(this);
        if (!b || !b->button)
            throw std::runtime_error("macOS: Missing radio binding.");
        [b->button setState:_selected ? NSControlStateValueOn
                                      : NSControlStateValueOff];
    }
    void radio::create() const {
        if (_created)
            return;
        auto *self = const_cast<radio *>(this);
        native_radio_view *b = [[native_radio_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        b->_nativeOwner = self;
        [b setButtonType:NSButtonTypeRadio];
        [b setTitle:text(_text)];
        [b setState:_selected ? NSControlStateValueOn
                              : NSControlStateValueOff];
        native_radio_target *t = [[native_radio_target alloc] init];
        t->_owner = self;
        [b setTarget:t];
        [b setAction:@selector(action:)];
        [parent(self) addSubview:b];
        auto *h = new mac::mac_radio();
        h->button = b;
        h->target = t;
        mac::radio_bindings.register_pair(self, h);
        _created = true;
        self->on_native_create();
    }
    void radio::show() const {
        auto *b = mac::radio_bindings.object_from_handle(
            const_cast<radio *>(this));
        if (!_created || !b || !b->button)
            throw std::runtime_error("macOS: radio is not created.");
        [b->button setHidden:NO];
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        auto *b = mac::radio_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            [b->button removeFromSuperview];
            [b->button release];
            [b->target release];
            mac::radio_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native
