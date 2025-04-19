#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>

namespace x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
    Display *cached_display = nullptr;
}
