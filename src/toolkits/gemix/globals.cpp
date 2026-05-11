#include "globals.h"

namespace gemix
{
    runtime_state runtime;
    native::bindings<WORD, native::wnd *> wnd_bindings;
    std::vector<native::button *> buttons;

    bool ensure_runtime()
    {
        if (runtime.initialized)
            return true;

        runtime.appl_id = appl_init();
        if (runtime.appl_id < 0)
            return false;

        runtime.vdi_handle = gem_compat_graf_handle4(
            &runtime.char_w, &runtime.char_h, &runtime.box_w, &runtime.box_h);
        v_opnvwk(runtime.work_in, &runtime.vdi_handle, runtime.work_out);
        if (runtime.vdi_handle == 0)
        {
            appl_exit();
            runtime.appl_id = -1;
            return false;
        }

        runtime.initialized = true;
        runtime.shutdown_requested = false;
        return true;
    }

    void shutdown_runtime()
    {
        if (!runtime.initialized)
            return;

        if (runtime.vdi_handle != 0)
        {
            v_clsvwk(runtime.vdi_handle);
            runtime.vdi_handle = 0;
        }

        appl_exit();
        runtime.appl_id = -1;
        runtime.initialized = false;
        runtime.shutdown_requested = false;
        wnd_bindings.clear();
        buttons.clear();
    }

    native::rect desktop_rect()
    {
        WORD x = 0;
        WORD y = 0;
        WORD w = 640;
        WORD h = 400;

        if (ensure_runtime())
            wind_get(0, WF_WORKXYWH, &x, &y, &w, &h);

        return native::rect(x, y, static_cast<native::dim>(w), static_cast<native::dim>(h));
    }
}
