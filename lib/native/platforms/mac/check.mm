//
// Implements the native AppKit check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#import <AppKit/AppKit.h>
#include <stdexcept>
#include <native.h>
#include <native/check.h>
#include "globals.h"
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
        NSWindow *w = p ? mac::wnd_bindings.handle_from_object(p) : nil;
        if (!p || !p->get_created() || !w || ![w contentView])
            throw std::runtime_error(
                "macOS: check requires a created parent.");
        return [w contentView];
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
    void check::create() const {
        if (_created)
            return;
        auto *self = const_cast<check *>(this);
        NSButton *b =
            [[NSButton alloc] initWithFrame:NSMakeRect(_bounds.p.x,
                                                       _bounds.p.y,
                                                       _bounds.d.w,
                                                       _bounds.d.h)];
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
        _created = true;
        self->on_wnd_create.emit();
    }
    void check::show() const {
        auto *b = mac::check_bindings.object_from_handle(
            const_cast<check *>(this));
        if (!_created || !b || !b->button)
            throw std::runtime_error("macOS: check is not created.");
        [b->button setHidden:NO];
    }
    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        auto *b = mac::check_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            [b->button removeFromSuperview];
            [b->button release];
            [b->target release];
            mac::check_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native
