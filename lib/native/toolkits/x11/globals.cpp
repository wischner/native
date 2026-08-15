//
// Implements the X11 shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace linux::x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
    native::bindings<native::wnd *, x11_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, x11_font *> font_bindings;
    native::bindings<Window,   x11_menu *> menu_bar_bindings;
    native::bindings<uint32_t, x11_menu *> menu_bindings;
    native::bindings<native::button *, x11_button *> button_bindings;
}
