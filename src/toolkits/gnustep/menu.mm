#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

@interface GNUMenuTarget : NSObject
{
@public
    native::app_wnd *_owner;
    int _item_id;
}
- (void)menuAction:(id)sender;
@end

@implementation GNUMenuTarget
- (void)menuAction:(id)sender
{
    (void)sender;
    if (_owner)
        _owner->on_menu.emit(_item_id);
}
@end

namespace native {

main_menu::~main_menu()
{
    if (!_id) return;
    auto *m = gnustep::menu_bindings.from_a(_id);
    if (m)
    {
        [m->ns_menu release];
        delete m;
    }
    gnustep::menu_bindings.unregister_by_a(_id);
    _id = 0;
}

void main_menu::attach(app_wnd &owner)
{
    if (_id || _tops.empty()) return;
    _owner = &owner;

    NSMenu *bar = [[NSMenu alloc] init];
    for (const auto &top : _tops)
    {
        NSMenuItem *top_item = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:top.title.c_str()]
                   action:nil
            keyEquivalent:@""];
        NSMenu *sub = [[NSMenu alloc]
            initWithTitle:[NSString stringWithUTF8String:top.title.c_str()]];

        for (const auto &item : top.items)
        {
            GNUMenuTarget *target = [[GNUMenuTarget alloc] init];
            target->_owner   = &owner;
            target->_item_id = item.id;

            NSMenuItem *mi = [[NSMenuItem alloc]
                initWithTitle:[NSString stringWithUTF8String:item.label.c_str()]
                       action:@selector(menuAction:)
                keyEquivalent:@""];
            [mi setTarget:target];
            [mi setRepresentedObject:target]; // keep alive
            [sub addItem:mi];
            [mi release];
        }

        [top_item setSubmenu:sub];
        [bar addItem:top_item];
        [sub release];
        [top_item release];
    }

    [NSApp setMainMenu:bar];

    auto *h    = new gnustep::gnustepmenu();
    h->ns_menu = bar; // bar is retained by NSApp
    h->owner   = &owner;
    _id = next_id();
    gnustep::menu_bindings.register_pair(_id, h);
}

} // namespace native
