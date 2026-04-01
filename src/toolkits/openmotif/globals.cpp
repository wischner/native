#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace motif
{
    XtAppContext app_instance = nullptr;
    bool exit_requested = false;

    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<Widget, native::wnd *> shell_bindings;
    native::bindings<Widget, native::wnd *> main_wnd_bindings;
    native::bindings<native::wnd *, motifgpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, motiffont *> font_bindings;
    native::bindings<uint32_t, motifmenu *> menu_bindings;
    native::bindings<native::button *, motifbutton *> button_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
}
