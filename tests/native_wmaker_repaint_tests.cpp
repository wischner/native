//
// Verifies buffered WINGs collection invalidation is deferred, coalesced,
// and cancelled when the native resource is destroyed or recreated.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <iostream>
#include <stdexcept>
#include "../lib/native/toolkits/wmaker/globals.h"

namespace
{
    void check(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
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
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
