//
// Implements the macOS menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

@interface NativeMenuTarget : NSObject
{
@public
    // Keep runtime type encoding independent of C++ class internals.
    void *_owner;
    int _item_id;
}
- (void)menuAction:(id)sender;
@end

@implementation NativeMenuTarget
- (void)menuAction:(id)sender
{
    (void)sender;
    auto *owner = static_cast<native::app_wnd *>(_owner);
    if (owner)
        owner->on_menu.emit(_item_id);
}
@end

namespace native {

main_menu::~main_menu() {
    detach();
}

void main_menu::detach() {
    if (!_id) {
        _owner = nullptr;
        return;
    }

    auto *m = mac::menu_bindings.object_from_handle(_id);
    if (m) {
        if ([NSApp mainMenu] == m->ns_menu)
            [NSApp setMainMenu:nil];
        [m->ns_menu release];
        delete m;
    }
    mac::menu_bindings.unregister_by_handle(_id);
    _id = 0;
    _owner = nullptr;
}

void main_menu::attach(app_wnd &owner) {
    if (_id || _tops.empty()) return;
    _owner = &owner;

    NSMenu *bar = [[NSMenu alloc] init];
    for (const auto &top : _tops) {
        NSMenuItem *top_item = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:top.title.c_str()]
                   action:nullptr
            keyEquivalent:@""];
        NSMenu *sub = [[NSMenu alloc]
            initWithTitle:[NSString stringWithUTF8String:top.title.c_str()]];
        for (const auto &item : top.items) {
            NativeMenuTarget *target = [[NativeMenuTarget alloc] init];
            target->_owner   = &owner;
            target->_item_id = item.id;
            NSMenuItem *mi = [[NSMenuItem alloc]
                initWithTitle:[NSString stringWithUTF8String:item.label.c_str()]
                       action:@selector(menuAction:)
                keyEquivalent:@""];
            [mi setTarget:target];
            [mi setRepresentedObject:target]; // keep alive
            [target release];
            [sub addItem:mi];
            [mi release];
        }
        [top_item setSubmenu:sub];
        [bar addItem:top_item];
        [sub release];
        [top_item release];
    }
    [NSApp setMainMenu:bar];

    auto *h   = new mac::mac_menu();
    h->ns_menu = bar; // bar is retained by NSApp
    h->owner   = &owner;
    _id = next_id();
    mac::menu_bindings.register_pair(_id, h);
}

} // namespace native
