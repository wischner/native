#pragma once

#include <Xm/Xm.h>

#include <native.h>
#include <bindings.h>

namespace x11
{
    extern native::bindings<Window, native::wnd *> wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;
}
