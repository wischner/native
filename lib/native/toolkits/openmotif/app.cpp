//
// Implements the OpenMotif application event-loop backend.
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
        if (!linux::openmotif::app_instance)
            throw std::runtime_error("Motif: No Xt application context "
                                     "available for main loop.");

        linux::openmotif::exit_requested = false;

        while (!linux::openmotif::exit_requested) {
            XEvent event;
            XtAppNextEvent(linux::openmotif::app_instance, &event);
            XtDispatchEvent(&event);
        }

        linux::openmotif::wnd_bindings.clear();
        linux::openmotif::shell_bindings.clear();
        linux::openmotif::wnd_gpx_bindings.clear();

        if (linux::openmotif::app_instance) {
            XtDestroyApplicationContext(linux::openmotif::app_instance);
            linux::openmotif::app_instance = nullptr;
        }

        linux::openmotif::cached_display = nullptr;
        linux::openmotif::wm_delete_window_atom = None;
        return 0;
    }

} // namespace native
