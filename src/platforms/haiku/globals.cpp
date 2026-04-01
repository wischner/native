#include <Application.h>
#include <View.h>
#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace haiku
{
    // Bind: application object.
    BApplication *global_app = nullptr;
    // Bind: BWindow to wnd.
    native::bindings<BWindow *, native::wnd *> wnd_bindings;
    // Bind: wnd to graphics cache.
    native::bindings<native::wnd *, haikugpx *> wnd_gpx_bindings;
    // Bind: font id to platform font handle.
    native::bindings<uint32_t, haikufont *> font_bindings;
    // Bind: menu id to menu handle.
    native::bindings<uint32_t, haikumenu *> menu_bindings;
    // Bind: owner app_wnd* to menu handle.
    native::bindings<native::app_wnd *, haikumenu *> owner_menu_bindings;
    // Bind: button owner pointer to native button handle.
    native::bindings<native::button *, haikubutton *> button_bindings;
}
