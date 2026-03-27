#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
    native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, x11font *> font_bindings;
    native::bindings<Window,   x11menu *> menubar_bindings;
    native::bindings<uint32_t, x11menu *> menu_bindings;
}
