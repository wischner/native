//
// Implements the native AppKit check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#import <AppKit/AppKit.h>
#include <algorithm>
#include <stdexcept>
#include <typeinfo>
#include <native.h>
#include <native/check.h>
#include "../../control_render_access.h"
#include "globals.h"

@interface native_check_view : NSButton {
@public
    void *_nativeOwner;
}
@end

@implementation native_check_view
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::check *>(_nativeOwner);
    if (!owner || !owner->get_created() ||
        typeid(*owner) == typeid(native::check)) {
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
@interface native_check_target : NSObject {
@public
    void *_owner;
}
- (void)action:(id)sender;
@end
@implementation native_check_target
- (void)action:(id)sender {
    auto *o = static_cast<native::check *>(_owner);
    if (o)
        o->on_native_checked([sender state] == NSControlStateValueOn);
}
@end
namespace
{
    NSString *text(const std::string &s) {
        NSString *v = [NSString stringWithUTF8String:s.c_str()];
        return v ? v : @"";
    }
    NSView *parent(native::check *c) {
        auto *p = c->get_parent();
        NSView *view = mac::parent_view(p, c);
        if (!view)
            throw std::runtime_error(
                "macOS: check requires a created parent.");
        return view;
    }
} // namespace
namespace native
{
    void check::apply_text() {
        auto *b = mac::check_bindings.object_from_handle(this);
        if (!b || !b->button)
            throw std::runtime_error("macOS: Missing check binding.");
        [b->button setTitle:text(_text)];
    }
    void check::apply_checked() {
        auto *b = mac::check_bindings.object_from_handle(this);
        if (!b || !b->button)
            throw std::runtime_error("macOS: Missing check binding.");
        [b->button setState:_checked ? NSControlStateValueOn
                                     : NSControlStateValueOff];
    }
    void check::create_native() {
        auto *self = this;
        native_check_view *b = [[native_check_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        b->_nativeOwner = self;
        [b setButtonType:NSButtonTypeSwitch];
        [b setTitle:text(_text)];
        [b setState:_checked ? NSControlStateValueOn
                             : NSControlStateValueOff];
        native_check_target *t = [[native_check_target alloc] init];
        t->_owner = self;
        [b setTarget:t];
        [b setAction:@selector(action:)];
        [parent(self) addSubview:b];
        auto *h = new mac::mac_check();
        h->button = b;
        h->target = t;
        mac::check_bindings.register_pair(self, h);
    }
    void check::show_native() {
        auto *b = mac::check_bindings.object_from_handle(
            this);
        if (!_created || !b || !b->button)
            throw std::runtime_error("macOS: check is not created.");
        [b->button setHidden:NO];
    }
    void check::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *b = mac::check_bindings.object_from_handle(self);
        if (b) {
            [b->button removeFromSuperview];
            [b->button release];
            [b->target release];
            mac::check_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native
