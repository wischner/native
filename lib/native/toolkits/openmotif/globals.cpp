//
// Implements the OpenMotif shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace linux::openmotif
{
    XtAppContext app_instance = nullptr;
    bool exit_requested = false;

    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<Widget, native::wnd *> shell_bindings;
    native::bindings<Widget, native::wnd *> main_wnd_bindings;
    native::bindings<native::wnd *, motif_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, motif_font *> font_bindings;
    native::bindings<uint32_t, motif_menu *> menu_bindings;
    native::bindings<native::button *, motif_button *> button_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
}
