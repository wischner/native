#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>

namespace motif
{
    native::bindings<Widget, native::wnd *> wnd_bindings;
}