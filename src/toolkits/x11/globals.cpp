#include <Xm/Xm.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None; // <- added to hold WM_DELETE_WINDOW atom
    native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings;
}
