//
// Implements the macOS button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>
#include <native/button.h>

#include "../../control_render_access.h"
#include "globals.h"

@interface native_button_view : NSButton {
@public
    void *_nativeOwner;
}
@end

@implementation native_button_view
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::button *>(_nativeOwner);
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

@interface mac_button_target : NSObject {
@public
    // Keep runtime type encoding independent of C++ class internals.
    void *_owner;
}
- (void)button_action:(id)sender;
@end

@implementation mac_button_target
- (void)button_action:(id)sender {
    (void)sender;
    auto *owner = static_cast<native::button *>(_owner);
    if (owner)
        owner->on_native_click();
}
@end

namespace
{
    // Convert a portable label into valid AppKit text.
    static NSString *to_nsstring(const std::string &text) {
        NSString *value = [NSString stringWithUTF8String:text.c_str()];
        if (!value)
            value = @"";
        return value;
    }
} // namespace

namespace native
{
    void button::apply_text() {
        auto *binding = mac::button_bindings.object_from_handle(this);
        if (!binding || !binding->ns_button)
            throw std::runtime_error(
                "macOS: Missing NSButton binding.");

        [binding->ns_button setTitle:to_nsstring(_text)];
    }

    void button::create() const {
        if (_created)
            return;

        wnd *p = get_parent();
        if (!p)
            throw std::runtime_error(
                "macOS: button requires a parent window.");
        if (!p->get_created())
            throw std::runtime_error(
                "macOS: button parent is not created.");

        NSView *content = mac::parent_view(
            p, const_cast<button *>(this));
        if (!content)
            throw std::runtime_error(
                "macOS: button parent has no content view.");

        native_button_view *btn = [[native_button_view alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        btn->_nativeOwner = const_cast<button *>(this);
        [btn setTitle:to_nsstring(_text)];
        [btn setButtonType:NSButtonTypeMomentaryPushIn];
        [btn setBezelStyle:NSBezelStyleRounded];
        [btn setControlSize:NSControlSizeRegular];
        [btn setFont:[NSFont systemFontOfSize:
                             [NSFont systemFontSizeForControlSize:
                                         NSControlSizeRegular]]];

        mac_button_target *target = [[mac_button_target alloc] init];
        target->_owner = const_cast<button *>(this);

        [btn setTarget:target];
        [btn setAction:@selector(button_action:)];
        [content addSubview:btn];

        auto *self = const_cast<button *>(this);
        auto *h = new mac::mac_button();
        h->ns_button = btn;
        h->target = target;
        h->owner = self;
        mac::button_bindings.register_pair(self, h);

        _created = true;
        self->on_native_create();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error(
                "macOS: Cannot show button before it is created.");

        auto *h = mac::button_bindings.object_from_handle(
            const_cast<button *>(this));
        if (!h || !h->ns_button)
            throw std::runtime_error(
                "macOS: Missing NSButton binding.");

        [h->ns_button setHidden:NO];
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = mac::button_bindings.object_from_handle(self);
        self->on_native_destroy();

        if (h) {
            if (h->ns_button) {
                [h->ns_button removeFromSuperview];
                [h->ns_button release];
            }
            if (h->target)
                [h->target release];

            mac::button_bindings.unregister_by_handle(self);
            delete h;
        }
    }
} // namespace native
