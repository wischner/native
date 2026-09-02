//
// Implements standard Windows message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <windows.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        UINT flags = MB_TASKMODAL;
        switch (buttons) {
        case message_box_buttons::ok: flags |= MB_OK; break;
        case message_box_buttons::ok_cancel: flags |= MB_OKCANCEL; break;
        case message_box_buttons::yes_no: flags |= MB_YESNO; break;
        case message_box_buttons::yes_no_cancel:
            flags |= MB_YESNOCANCEL;
            break;
        }
        switch (icon) {
        case message_box_icon::none: break;
        case message_box_icon::information:
            flags |= MB_ICONINFORMATION;
            break;
        case message_box_icon::warning: flags |= MB_ICONWARNING; break;
        case message_box_icon::error: flags |= MB_ICONERROR; break;
        case message_box_icon::question: flags |= MB_ICONQUESTION; break;
        }
        HWND window = windows::wnd_bindings.handle_from_object(&owner);
        const int result = MessageBoxW(
            window,
            windows::utf8_to_wide(message).c_str(),
            windows::utf8_to_wide(title).c_str(),
            flags);
        switch (result) {
        case IDOK: return message_box_result::ok;
        case IDCANCEL: return message_box_result::cancel;
        case IDYES: return message_box_result::yes;
        case IDNO: return message_box_result::no;
        default: return message_box_result::none;
        }
    }
} // namespace native
