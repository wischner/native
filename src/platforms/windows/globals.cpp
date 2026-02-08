#include <windows.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace win
{
    native::bindings<HWND, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, wingpx *> wnd_gpx_bindings;
}
