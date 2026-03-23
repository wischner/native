#include <stdexcept>

#include <X11/Intrinsic.h>

#include <native.h>

#include "globals.h"

namespace native
{

    int app::main_loop()
    {
        if (!motif::app_instance)
            throw std::runtime_error("Motif: No Xt application context available for main loop.");

        motif::exit_requested = false;

        while (!motif::exit_requested)
        {
            XEvent event;
            XtAppNextEvent(motif::app_instance, &event);
            XtDispatchEvent(&event);
        }

        motif::wnd_bindings.clear();
        motif::shell_bindings.clear();
        motif::wnd_gpx_bindings.clear();

        if (motif::app_instance)
        {
            XtDestroyApplicationContext(motif::app_instance);
            motif::app_instance = nullptr;
        }

        motif::cached_display = nullptr;
        motif::wm_delete_window_atom = None;
        return 0;
    }

} // namespace native
