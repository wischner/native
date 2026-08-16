//
// Implements the X11 application event loop through Xt so Athena
// widgets receive their standard translations, callbacks, and popup
// behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>

#include <native.h>
#include <native/app.h>

#include "globals.h"

namespace native
{
    int app::main_loop() {
        if (!linux::x11::app_instance)
            throw std::runtime_error(
                "X11/Athena: No Xt application context.");

        linux::x11::exit_requested = false;

        while (!linux::x11::exit_requested) {
            XEvent event;
            XtAppNextEvent(linux::x11::app_instance, &event);
            XtDispatchEvent(&event);
        }

        linux::x11::wnd_bindings.clear();
        linux::x11::shell_bindings.clear();
        linux::x11::main_wnd_bindings.clear();
        linux::x11::wnd_gpx_bindings.clear();

        if (linux::x11::app_instance) {
            XtDestroyApplicationContext(linux::x11::app_instance);
            linux::x11::app_instance = nullptr;
        }

        linux::x11::cached_display = nullptr;
        linux::x11::wm_delete_window_atom = None;
        return 0;
    }
} // namespace native
