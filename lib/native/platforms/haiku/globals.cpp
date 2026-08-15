//
// Implements the Haiku shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

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
    native::bindings<native::wnd *, haiku_gpx *> wnd_gpx_bindings;
    // Bind: font id to platform font handle.
    native::bindings<uint32_t, haiku_font *> font_bindings;
    // Bind: menu id to menu handle.
    native::bindings<uint32_t, haiku_menu *> menu_bindings;
    // Bind: owner app_wnd* to menu handle.
    native::bindings<native::app_wnd *, haiku_menu *> owner_menu_bindings;
    // Bind: button owner pointer to native button handle.
    native::bindings<native::button *, haiku_button *> button_bindings;
}
