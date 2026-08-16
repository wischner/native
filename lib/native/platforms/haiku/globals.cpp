//
// Implements the Haiku shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Application.h>
#include <CheckBox.h>
#include <ListView.h>
#include <RadioButton.h>
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
    native::bindings<native::app_wnd *, haiku_menu *>
        owner_menu_bindings;
    // Bind: button owner pointer to native button handle.
    native::bindings<native::button *, haiku_button *> button_bindings;
    native::bindings<native::check *, haiku_check *> check_bindings;
    native::bindings<native::radio *, haiku_radio *> radio_bindings;
    native::bindings<native::list *, haiku_list *> list_bindings;
    native::bindings<native::file_dialog *, haiku_file_dialog *>
        file_dialog_bindings;

    BView *view_from_control(native::wnd *control) {
        if (auto *button = dynamic_cast<native::button *>(control)) {
            auto *binding = button_bindings.object_from_handle(button);
            return binding ? binding->button : nullptr;
        }
        if (auto *check = dynamic_cast<native::check *>(control)) {
            auto *binding = check_bindings.object_from_handle(check);
            return binding ? binding->view : nullptr;
        }
        if (auto *radio = dynamic_cast<native::radio *>(control)) {
            auto *binding = radio_bindings.object_from_handle(radio);
            return binding ? binding->view : nullptr;
        }
        if (auto *list = dynamic_cast<native::list *>(control)) {
            auto *binding = list_bindings.object_from_handle(list);
            return binding ? binding->view : nullptr;
        }
        return nullptr;
    }
} // namespace haiku
