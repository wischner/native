//
// Verifies buffered WINGs collection invalidation is deferred, coalesced,
// and cancelled when the native resource is destroyed or recreated.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <X11/keysym.h>
#include <iostream>
#include <stdexcept>
#include "../lib/native/toolkits/wmaker/globals.h"

namespace
{
    void check(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // Exercise the production event loop under continuous posted refresh.
    // The key behind a property burst must arrive before refresh repeats
    // once per notification; no wall-clock timing assumption is needed.
    void check_event_fairness() {
        native::app_wnd window("Event fairness", 0, 0, 320, 240);
        native::canvas surface(0, 0, 100, 100);
        surface.set_parent(&window);
        unsigned refreshes = 0;
        bool received_key = false;
        std::function<void()> refresh;
        refresh = [&] {
            if (++refreshes > 32) {
                window.request_close();
                return;
            }
            surface.invalidate();
            native::app::post(refresh);
        };
        window.on_key.connect([&](native::key_event event) {
            if (event.key != native::key_code::f10) return false;
            received_key = true;
            window.request_close();
            return true;
        });
        window.on_wnd_create.connect([&] {
            surface.create();
            surface.show();
            auto *display = linux::wmaker::display;
            const Window target = linux::wmaker::drawable(&window);
            XEvent event{};
            event.xproperty.type = PropertyNotify;
            event.xproperty.display = display;
            event.xproperty.window = target;
            event.xproperty.atom = XInternAtom(
                display, "NATIVE_FAIRNESS_TEST", False);
            event.xproperty.state = PropertyNewValue;
            for (int index = 0; index < 256; ++index)
                XSendEvent(display, target, False, NoEventMask, &event);
            event = {};
            event.xkey.type = KeyPress;
            event.xkey.display = display;
            event.xkey.window = target;
            event.xkey.root = DefaultRootWindow(display);
            event.xkey.keycode = XKeysymToKeycode(display, XK_F10);
            event.xkey.same_screen = True;
            XSendEvent(display, target, False, NoEventMask, &event);
            XFlush(display);
            native::app::post(refresh);
            return false;
        });
        native::app::run(window);
        check(received_key && refreshes < 16,
              "posted refresh starved keyboard input behind notifications");
    }

    // Exercise the same frame host used by icon and tree collections.
    void check_repaint(native::wnd &control) {
        int paints = 0;
        auto connection = control.on_wnd_paint.connect_scoped([&](native::wnd_paint_event) {
            ++paints;
            return false;
        });
        control.create();
        control.show();
        linux::wmaker::dispatch_deferred();
        paints = 0;
        control.invalidate();
        control.invalidate(native::rect(0, 0, 10, 10));
        control.invalidate();
        check(paints == 0, "invalidation painted synchronously");
        linux::wmaker::dispatch_deferred();
        check(paints == 1, "invalidation burst was not coalesced");
        XSync(linux::wmaker::display, False);
        XEvent reply{};
        check(!XCheckTypedWindowEvent(linux::wmaker::display,
                  linux::wmaker::drawable(&control), NoExpose, &reply),
              "buffer presentation generated a redundant NoExpose reply");
        control.invalidate();
        control.destroy();
        control.create();
        control.show();
        linux::wmaker::dispatch_deferred();
        paints = 0;
        control.invalidate();
        linux::wmaker::dispatch_deferred();
        check(paints == 1, "recreated control did not repaint");
        control.invalidate();
        control.destroy();
        paints = 0;
        linux::wmaker::dispatch_deferred();
        check(paints == 0, "destroyed control received a repaint");
    }
}

int program(int, char **) {
    try {
        native::app_wnd window("Collection repaint", 0, 0, 320, 240);
        window.create();
        native::icon_view icons({}, 0, 0, 100, 100);
        icons.set_parent(&window);
        check_repaint(icons);
        native::tree_view tree;
        tree.set_parent(&window);
        tree.set_bounds(native::rect(110, 0, 100, 100));
        check_repaint(tree);
        window.destroy();
        check_event_fairness();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
