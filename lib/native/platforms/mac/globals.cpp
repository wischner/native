//
// Implements the macOS shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"
#include <native.h>
#include <bindings.h>
#import <Cocoa/Cocoa.h>

namespace mac
{
    NSApplication *global_app = nullptr;
    native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, id> delegate_bindings;
    native::bindings<native::wnd *, mac_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, mac_font *> font_bindings;
    native::bindings<uint32_t, mac_menu *> menu_bindings;
    native::bindings<native::button *, mac_button *> button_bindings;
}
