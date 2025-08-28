#include <Application.h>
#include <native.h>
#include <bindings.h>

namespace haiku
{
    // Bind: application object.
    BApplication *global_app = new BApplication("application/x-vnd.native-app");
    // Bind: BWindow to wnd.
    native::bindings<BWindow *, native::wnd *> wnd_bindings;
}
