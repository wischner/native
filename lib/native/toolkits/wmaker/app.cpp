//
// Implements the application event loop through the WINGs dispatcher.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native/app.h>

#include "globals.h"
#include "../../post_backend.h"

namespace native
{
    int app::main_loop() {
        if (!linux::wmaker::initialized || !linux::wmaker::screen ||
            !linux::wmaker::display) {
            throw std::runtime_error(
                "Window Maker/WINGs: no application screen.");
        }

        linux::wmaker::exit_requested = false;
        detail::drain_posted_work();
        while (!linux::wmaker::exit_requested) {
            XEvent event = {};
            WMNextEvent(linux::wmaker::display, &event);
            if (!linux::wmaker::handle_menu_event(event))
                WMHandleEvent(&event);
            linux::wmaker::dispatch_deferred();
        }

        linux::wmaker::wnd_bindings.clear();
        linux::wmaker::window_bindings.clear();
        linux::wmaker::graphics_bindings.clear();
        if (linux::wmaker::list_selection_background) {
            WMReleaseColor(
                linux::wmaker::list_selection_background);
            linux::wmaker::list_selection_background = nullptr;
        }
        if (linux::wmaker::list_selection_text) {
            WMReleaseColor(linux::wmaker::list_selection_text);
            linux::wmaker::list_selection_text = nullptr;
        }
        linux::wmaker::screen = nullptr;
        linux::wmaker::display = nullptr;
        linux::wmaker::initialized = false;
        WMReleaseApplication();
        return 0;
    }
} // namespace native
