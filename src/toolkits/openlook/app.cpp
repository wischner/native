#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <xview/notify.h>
#ifdef coord
#undef coord
#endif

#include <native.h>

#include "globals.h"

namespace native
{
    namespace
    {
        bool trace_enabled()
        {
            static int enabled = [] {
                const char *value = std::getenv("NATIVE_OPENLOOK_TRACE");
                return (value && *value && std::strcmp(value, "0") != 0) ? 1 : 0;
            }();
            return enabled != 0;
        }

        void trace(const char *msg)
        {
            if (!trace_enabled())
                return;
            std::fprintf(stderr, "[openlook] %s\n", msg);
            std::fflush(stderr);
        }
    } // namespace

    int app::main_loop()
    {
        trace("app::main_loop enter");
        if (!openlook::xview_initialized)
            throw std::runtime_error("OpenLook: XView is not initialized for main loop.");

        openlook::exit_requested = false;

        trace("before notify_start");
        Notify_error err = notify_start();
        trace("after notify_start");
        if (err != NOTIFY_OK && err != NOTIFY_NOT_STARTED)
            throw std::runtime_error("OpenLook: notify_start failed.");

        openlook::frame_bindings.clear();
        openlook::canvas_bindings.clear();
        openlook::wnd_gpx_bindings.clear();

        openlook::cached_display = nullptr;
        openlook::xview_initialized = false;
        trace("app::main_loop exit");

        return 0;
    }

} // namespace native
