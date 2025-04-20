#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>

namespace x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None; // <- added to hold WM_DELETE_WINDOW atom
}
