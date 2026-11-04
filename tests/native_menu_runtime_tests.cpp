//
// Checks live menu marks, silent setters, native command callbacks and
// retained check state across menu destruction and reattachment.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <iostream>
#include <stdexcept>

#if defined(_WIN32)
#include "../lib/native/platforms/windows/globals.h"
#elif defined(__APPLE__)
#include "../lib/native/platforms/mac/globals.h"
#elif defined(__HAIKU__)
#include "../lib/native/platforms/haiku/globals.h"
#include <Autolock.h>
#include <MenuBar.h>
#include <MenuItem.h>
#elif defined(MENU_TEST_SDL2)
#include "../lib/native/toolkits/sdl2/globals.h"
#elif defined(MENU_TEST_X11)
#include "../lib/native/toolkits/x11/globals.h"
#include <X11/StringDefs.h>
#include <X11/Xaw/SmeBSB.h>
#elif defined(MENU_TEST_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#include <Xm/ToggleB.h>
#elif defined(MENU_TEST_OPENLOOK)
#include "../lib/native/toolkits/openlook/globals.h"
#elif defined(MENU_TEST_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#elif defined(MENU_TEST_GEMIX)
#include "../lib/native/toolkits/gemix/globals.h"
#elif defined(MENU_TEST_WAYLAND) || defined(MENU_TEST_ELEMENTARY)
#include "../lib/native/toolkits/wayland/globals.h"
#endif

// Check native canvas input alongside window/menu activation.
void check_canvas_keyboard(native::app_wnd &parent);
void check_window_title(native::app_wnd &parent);

namespace
{
    using namespace native;

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    // Read the native resource, independently of the portable getter.
    bool native_checked(app_wnd &owner, int command) {
#if defined(_WIN32)
        auto *peer = windows::menu_bindings.object_from_handle(owner.menu.id());
        return (GetMenuState(peer->hmenu, command, MF_BYCOMMAND) &
                MF_CHECKED) != 0;
#elif defined(__APPLE__)
        auto *peer = mac::menu_bindings.object_from_handle(owner.menu.id());
        for (NSMenuItem *top in [peer->ns_menu itemArray]) {
            NSMenuItem *item = [[top submenu] itemWithTag:command];
            if (item) return [item state] == NSControlStateValueOn;
        }
#elif defined(__HAIKU__)
        auto *peer = haiku::menu_bindings.object_from_handle(owner.menu.id());
        BAutolock lock(peer->bar->Window());
        expect(lock.IsLocked(), "lock menu window");
        return peer->bar->FindItem(static_cast<uint32>(command))->IsMarked();
#elif defined(MENU_TEST_SDL2) || defined(MENU_TEST_WMAKER)
#if defined(MENU_TEST_SDL2)
        auto *peer = linux::sdl2::menu_bindings.object_from_handle(owner.menu.id());
#else
        auto *peer = linux::wmaker::menu_bindings.object_from_handle(owner.menu.id());
#endif
        for (const auto &top : peer->tops)
            for (const auto &item : top.items)
                if (item.id == command) return item.checked;
#elif defined(MENU_TEST_X11)
        auto *peer = linux::x11::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks) {
            if (callback->item_id != command) continue;
            Pixmap mark = None;
            XtVaGetValues(callback->item, XtNleftBitmap, &mark, nullptr);
            return mark != None;
        }
#elif defined(MENU_TEST_MOTIF)
        auto *peer = linux::openmotif::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks)
            if (callback->item_id == command)
                return XmToggleButtonGetState(callback->item);
#elif defined(MENU_TEST_OPENLOOK)
        auto *peer = linux::openlook::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks)
            if (callback->item_id == command)
                return xv_get(callback->item, MENU_SELECTED) != 0;
#elif defined(MENU_TEST_GEMIX)
        auto &peer = linux::gemix::menu_states.at(&owner);
        for (const auto &[object, id] : peer.object_to_item_id)
            if (id == command) return (peer.tree[object].ob_state & CHECKED) != 0;
#elif defined(MENU_TEST_WAYLAND) || defined(MENU_TEST_ELEMENTARY)
        auto &peer = linux::wayland::state(owner);
        const auto name = "command" + std::to_string(command);
        GVariant *state = g_action_group_get_action_state(
            G_ACTION_GROUP(peer.menu_actions), name.c_str());
        expect(state != nullptr, "checkable GTK action has boolean state");
        const bool result = g_variant_get_boolean(state);
        g_variant_unref(state);
        return result;
#endif
        throw std::runtime_error("native menu command missing");
    }

    // Exercise toolkit callbacks where they can be invoked synchronously.
    void activate(app_wnd &owner, int command) {
#if defined(_WIN32)
        SendMessageW(windows::wnd_bindings.handle_from_object(&owner),
                     WM_COMMAND, MAKEWPARAM(command, command == 42 ? 1 : 0), 0);
#elif defined(__APPLE__)
        auto *peer = mac::menu_bindings.object_from_handle(owner.menu.id());
        for (NSMenuItem *top in [peer->ns_menu itemArray]) {
            NSMenu *sub = [top submenu];
            const NSInteger index = [sub indexOfItemWithTag:command];
            if (index >= 0) { [sub performActionForItemAtIndex:index]; return; }
        }
#elif defined(MENU_TEST_X11)
        auto *peer = linux::x11::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks)
            if (callback->item_id == command) {
                XtCallCallbacks(callback->item, XtNcallback, nullptr);
                return;
            }
#elif defined(MENU_TEST_MOTIF)
        auto *peer = linux::openmotif::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks)
            if (callback->item_id == command) {
                XmToggleButtonSetState(callback->item,
                    !XmToggleButtonGetState(callback->item), True);
                return;
            }
#elif defined(MENU_TEST_OPENLOOK)
        auto *peer = linux::openlook::menu_bindings.object_from_handle(owner.menu.id());
        for (auto *callback : peer->callbacks)
            if (callback->item_id == command) {
                using notify = Xv_opaque (*)(Menu, Menu_item);
                auto action = reinterpret_cast<notify>(
                    xv_get(callback->item, MENU_NOTIFY_PROC));
                xv_set(callback->item, MENU_SELECTED,
                       !owner.menu.get_checked(command), nullptr);
                action(peer->menus.front(), callback->item);
                return;
            }
#elif defined(MENU_TEST_SDL2)
        auto *peer = linux::sdl2::menu_bindings.object_from_handle(owner.menu.id());
        if (command == 42) {
            SDL_KeyboardEvent event{};
            event.type = SDL_KEYDOWN;
            event.keysym.sym = SDLK_g;
            event.keysym.mod = KMOD_CTRL;
            expect(linux::sdl2::handle_menu_key(peer, event),
                   "SDL shortcut activates checkable command");
        } else {
            owner.on_native_menu(command);
        }
#elif defined(MENU_TEST_WAYLAND) || defined(MENU_TEST_ELEMENTARY)
        const auto name = "native.command" + std::to_string(command);
        expect(gtk_widget_activate_action(linux::wayland::state(owner).widget,
                   name.c_str(), nullptr), "GTK menu action activates");
#else
        owner.on_native_menu(command);
#endif
    }

    class menu_window final : public modeless_wnd
    {
    public:
        using modeless_wnd::modeless_wnd;
        bool input_enabled = true;

        bool get_input_enabled() const override {
            return input_enabled && modeless_wnd::get_input_enabled();
        }
    };

#if defined(MENU_TEST_SDL2)
    // A changed model must also produce a visible tick in the popup gutter.
    void check_pixels(app_wnd &owner) {
        auto *peer = linux::sdl2::menu_bindings.object_from_handle(owner.menu.id());
        peer->open_idx = 0;
        peer->popup_x = 0;
        peer->popup_y = 24;
        img unchecked(300, 150), checked(300, 150);
        owner.menu.set_checked(42, false);
        linux::sdl2::render_menu(peer, unchecked.get_gpx(), 300, 150);
        owner.menu.set_checked(42, true);
        linux::sdl2::render_menu(peer, checked.get_gpx(), 300, 150);
        int changed = 0;
        for (int y = 27; y < 43; ++y)
            for (int x = 4; x < 17; ++x)
                if (unchecked.pixels()[y * 300 + x] !=
                    checked.pixels()[y * 300 + x]) ++changed;
        expect(changed > 0, "checked SDL popup paints a visible tick");
        expect(linux::sdl2::handle_menu_click(peer, 35, 34, 300),
               "SDL pointer activates the checked row");
        expect(!owner.menu.get_checked(42), "SDL pointer clears the tick");
    }
#endif

    class fixture final : public app_wnd
    {
    public:
        bool failed = false;
        menu_window child{*this, "Checkable menus", 60, 60, 360, 240};

        fixture() : app_wnd("Menu test owner", 10, 10, 400, 300) {
            menu << "&File" << menu_items("Main");
            child.menu << "&View" << (menu_items(
                menu_check_item{42, "&Grid\tCtrl+G", true})
                << menu_check_item{43, "&Rulers", false}
                << menu_separator
                << std::make_pair(44, std::string("Refresh")));
            on_wnd_create.connect([this] {
                app::post([this] { check(); });
                return true;
            });
        }

    private:
        void check() {
            try {
                check_canvas_keyboard(*this);
                check_window_title(*this);
                child.create();
                child.show();
#if defined(__APPLE__)
                auto *main_peer = mac::menu_bindings.object_from_handle(menu.id());
                auto *child_peer = mac::menu_bindings.object_from_handle(child.menu.id());
                expect([[main_peer->ns_menu itemAtIndex:1].title isEqualToString:@"File"],
                       "File remains separate from the AppKit application menu");
                NSWindow *main_window = mac::wnd_bindings.handle_from_object(this);
                NSWindow *child_window = mac::wnd_bindings.handle_from_object(&child);
                [child_window makeKeyAndOrderFront:nil];
                // An inactive test process may not activate AppKit windows.
                // Still exercise the real delegate notification path.
                if (![child_window isKeyWindow])
                    [[child_window delegate] windowDidBecomeKey:
                        [NSNotification notificationWithName:NSWindowDidBecomeKeyNotification
                                                      object:child_window]];
                expect([NSApp mainMenu] == child_peer->ns_menu, "child menu activates");
                [main_window makeKeyAndOrderFront:nil];
                // A locked test desktop cannot activate AppKit windows.
                // Still exercise the real delegate notification path.
                if (![main_window isKeyWindow])
                    [[main_window delegate] windowDidBecomeKey:
                        [NSNotification notificationWithName:NSWindowDidBecomeKeyNotification
                                                      object:main_window]];
                expect([NSApp mainMenu] == main_peer->ns_menu, "main menu restores on activation");
                [child_window makeKeyAndOrderFront:nil];
                // A locked test desktop cannot activate AppKit windows.
                // Still exercise the real delegate notification path.
                if (![child_window isKeyWindow])
                    [[child_window delegate] windowDidBecomeKey:
                        [NSNotification notificationWithName:NSWindowDidBecomeKeyNotification
                                                      object:child_window]];
                expect([NSApp mainMenu] == child_peer->ns_menu, "child menu reactivates");
#endif
                expect(native_checked(child, 42), "initial native mark");
                expect(!native_checked(child, 43), "initial unchecked item");
#if defined(MENU_TEST_SDL2)
                check_pixels(child);
#endif
                int commands = 0;
                bool observed = false;
                child.on_menu.connect([&](int command) {
                    ++commands;
                    observed = child.menu.get_checked(command);
                    expect(native_checked(child, command) == observed,
                           "native mark synchronized before notification");
                    return true;
                });
                child.menu.set_checked(42, false).set_checked(43, true);
                expect(commands == 0 && !native_checked(child, 42) &&
                       native_checked(child, 43), "live silent updates");
                activate(child, 42);
                expect(commands == 1 && observed, "activation checks once");
                activate(child, 42);
                expect(commands == 2 && !observed, "activation unchecks once");
                child.input_enabled = false;
                activate(child, 42);
                expect(commands == 2 && !child.menu.get_checked(42) &&
                       !native_checked(child, 42), "blocked input restores mark");
                child.input_enabled = true;
                child.menu.set_checked(42, true);
                child.destroy();
                expect(child.menu.get_checked(42), "destroy retains model");
                child.create();
                child.show();
                expect(native_checked(child, 42) && native_checked(child, 43),
                       "recreated menu restores native marks");
                activate(child, 42);
                expect(commands == 3 && !observed, "recreated callback works");
                child.on_menu.connect([this](int) {
                    child.destroy();
                    return true;
                });
                activate(child, 42);
                expect(!child.get_created(), "command handler can destroy menu");
                std::cout << "Menu checks passed\n";
            } catch (const std::exception &error) {
                failed = true;
                std::cerr << error.what() << '\n';
            }
            destroy();
        }
    };
} // namespace

int program(int, char **) {
    fixture window;
    const int result = app::run(window);
    return window.failed ? 1 : result;
}
