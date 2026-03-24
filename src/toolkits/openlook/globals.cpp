#include <X11/Xlib.h>

#include <xview/xview.h>
#ifdef coord
#undef coord
#endif

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace openlook
{
    bool xview_initialized = false;
    bool exit_requested = false;

    native::bindings<Xv_opaque, native::wnd *> frame_bindings;
    native::bindings<Xv_opaque, native::wnd *> canvas_bindings;
    native::bindings<native::wnd *, openlookgpx *> wnd_gpx_bindings;

    Display *cached_display = nullptr;
}
