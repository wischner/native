//
// Implements standard OpenLook notice dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include <xview/xview.h>
#include <xview/notice.h>

#include "globals.h"

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon) {
        auto *owner_state = linux::openlook::window_state(&owner);
        if (!owner_state || !owner_state->paint_window)
            throw std::runtime_error(
                "OpenLook/XView: Message notice has no owner.");
        const std::string text = title.empty()
            ? message : title + "\n\n" + message;
        int result = 0;
        switch (buttons) {
        case message_box_buttons::ok:
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON, "OK", 1,
                nullptr);
            return result == 1 ? message_box_result::ok
                               : message_box_result::none;
        case message_box_buttons::ok_cancel:
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON, "OK", 1,
                NOTICE_BUTTON, "Cancel", 2,
                nullptr);
            return result == 1 ? message_box_result::ok
                 : result == 2 ? message_box_result::cancel
                               : message_box_result::none;
        case message_box_buttons::yes_no:
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON, "Yes", 3,
                NOTICE_BUTTON, "No", 4,
                nullptr);
            return result == 3 ? message_box_result::yes
                 : result == 4 ? message_box_result::no
                               : message_box_result::none;
        case message_box_buttons::yes_no_cancel:
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON, "Yes", 3,
                NOTICE_BUTTON, "No", 4,
                NOTICE_BUTTON, "Cancel", 2,
                nullptr);
            return result == 3 ? message_box_result::yes
                 : result == 4 ? message_box_result::no
                 : result == 2 ? message_box_result::cancel
                               : message_box_result::none;
        }
        return message_box_result::none;
    }
} // namespace native
