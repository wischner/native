#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace motif
{
    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, motifgpx *> wnd_gpx_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
}
