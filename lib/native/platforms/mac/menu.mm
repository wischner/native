//
// Implements the macOS menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#include <native.h>
#include <native/menu.h>
#include "globals.h"
#include "../../menu_shortcut.h"

namespace
{
    uint32_t next_id() {
        static uint32_t c = 0;
        return ++c;
    }
} // namespace

@interface native_menu_target : NSObject {
@public
    // Keep runtime type encoding independent of C++ class internals.
    void *_owner;
    int _item_id;
}
- (void)menu_action:(id)sender;
- (void)quit_action:(id)sender;
@end

@implementation native_menu_target
- (void)quit_action:(id)sender {
    (void)sender;
    if (auto *owner = native::app::main_wnd()) owner->request_close();
}
- (void)menu_action:(id)sender {
    (void)sender;
    auto *owner = static_cast<native::app_wnd *>(_owner);
    if (owner && owner->get_input_enabled())
        owner->on_native_menu(_item_id);
}
@end

namespace mac
{
    void activate_menu(native::app_wnd *owner) {
        for (auto *window = owner; window; window = window->get_owner()) {
            auto *menu = menu_bindings.object_from_handle(window->menu.id());
            if (menu) {
                [NSApp setMainMenu:menu->ns_menu];
                return;
            }
        }
        [NSApp setMainMenu:nil];
    }
}

namespace native
{

    void main_menu::apply_checked(int command) {
        auto *menu = mac::menu_bindings.object_from_handle(_id);
        if (!menu) return;
        for (NSMenuItem *top in [menu->ns_menu itemArray]) {
            NSMenuItem *item = [[top submenu] itemWithTag:command];
            if (item)
                [item setState:get_checked(command)
                    ? NSControlStateValueOn : NSControlStateValueOff];
        }
    }

    void main_menu::apply_enabled(int command) {
        auto *menu = mac::menu_bindings.object_from_handle(_id);
        if (!menu) return;
        for (NSMenuItem *top in [menu->ns_menu itemArray]) {
            NSMenuItem *item = [[top submenu] itemWithTag:command];
            if (item)
                [item setEnabled:get_enabled(command) ? YES : NO];
        }
    }

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
                mac::activate_menu(_owner ? _owner->get_owner() : nullptr);
            [m->ns_menu release];
            delete m;
        }
        mac::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;
        _owner = &owner;

        NSMenu *bar = [[NSMenu alloc] init];
        // AppKit reserves the first submenu for the application menu.
        // Supplying File there silently renames it to the application name.
        NSMenuItem *application_item = [[NSMenuItem alloc] init];
        NSMenu *application_menu = [[NSMenu alloc] initWithTitle:@""];
        native_menu_target *quit_target = [[native_menu_target alloc] init];
        NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
        if (!name) name = [[NSProcessInfo processInfo] processName];
        NSMenuItem *quit = [[NSMenuItem alloc]
            initWithTitle:[@"Quit " stringByAppendingString:name]
                   action:@selector(quit_action:) keyEquivalent:@"q"];
        [quit setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [quit setTarget:quit_target];
        [quit setRepresentedObject:quit_target];
        [application_menu addItem:quit];
        [application_item setSubmenu:application_menu];
        [bar addItem:application_item];
        [quit release];
        [quit_target release];
        [application_menu release];
        [application_item release];
        for (const auto &top : _tops) {
            NSMenuItem *top_item = [[NSMenuItem alloc]
                initWithTitle:[NSString
                                  stringWithUTF8String:top.title
                                                           .c_str()]
                       action:nullptr
                keyEquivalent:@""];
            NSMenu *sub = [[NSMenu alloc]
                initWithTitle:[NSString
                                  stringWithUTF8String:top.title
                                                           .c_str()]];
            // AppKit otherwise recomputes enabled state from each target.
            [sub setAutoenablesItems:NO];
            for (const auto &item : top.items) {
                if (item.separator) {
                    [sub addItem:[NSMenuItem separatorItem]];
                    continue;
                }
                native_menu_target *target =
                    [[native_menu_target alloc] init];
                target->_owner = &owner;
                target->_item_id = item.id;
                const auto parsed = native::detail::parse_menu_shortcut(
                    item.shortcut);
                NSString *key = parsed.key.size() == 1
                    ? [[NSString stringWithUTF8String:parsed.key.c_str()]
                          lowercaseString]
                    : @"";
                if (parsed.key.size() >= 2 &&
                    (parsed.key[0] == 'F' || parsed.key[0] == 'f')) {
                    int number = 0;
                    for (std::size_t i = 1; i < parsed.key.size(); ++i) {
                        const char digit = parsed.key[i];
                        if (digit < '0' || digit > '9' || number > 35) {
                            number = 0;
                            break;
                        }
                        number = number * 10 + digit - '0';
                    }
                    if (number >= 1 && number <= 35) {
                        const unichar value = NSF1FunctionKey + number - 1;
                        key = [NSString stringWithCharacters:&value length:1];
                    }
                }
                NSMenuItem *mi = [[NSMenuItem alloc]
                    initWithTitle:[NSString
                                      stringWithUTF8String:item.label
                                                               .c_str()]
                           action:@selector(menu_action:)
                    keyEquivalent:key];
                NSEventModifierFlags modifiers = 0;
                if (parsed.control) modifiers |= NSEventModifierFlagControl;
                if (parsed.alt) modifiers |= NSEventModifierFlagOption;
                if (parsed.shift) modifiers |= NSEventModifierFlagShift;
                if (parsed.command) modifiers |= NSEventModifierFlagCommand;
                [mi setKeyEquivalentModifierMask:modifiers];
                [mi setTag:item.id];
                [mi setState:item.checked
                    ? NSControlStateValueOn : NSControlStateValueOff];
                [mi setEnabled:item.enabled ? YES : NO];
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

        auto *h = new mac::mac_menu();
        h->ns_menu = bar; // bar is retained by NSApp
        h->owner = &owner;
        _id = next_id();
        mac::menu_bindings.register_pair(_id, h);
        if ([NSApp keyWindow] == mac::wnd_bindings.handle_from_object(&owner))
            mac::activate_menu(&owner);
    }

} // namespace native
