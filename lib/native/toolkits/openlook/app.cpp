//
// Implements the application event loop with the XView notifier so
// native OPEN LOOK controls retain their standard event translations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/app.h>

#include <xview/window.h>

#include "globals.h"

namespace native
{
    int app::main_loop() {
        if (!linux::openlook::initialized ||
            !linux::openlook::main_frame) {
            throw std::runtime_error(
                "OpenLook/XView: no main frame is available.");
        }

        linux::openlook::exit_requested = false;
        xv_main_loop(linux::openlook::main_frame);

        linux::openlook::wnd_bindings.clear();
        linux::openlook::frame_bindings.clear();
        linux::openlook::window_bindings.clear();
        linux::openlook::wnd_gpx_bindings.clear();
        linux::openlook::main_frame = XV_NULL;
        return 0;
    }
} // namespace native
