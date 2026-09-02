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
#include "../../message_box_common.h"

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon) {
        detail::validate_message_box_owner(owner);
        auto *owner_state = linux::openlook::window_state(&owner);
        if (!owner_state || !owner_state->paint_window)
            throw std::runtime_error(
                "OpenLook/XView: Message notice has no owner.");
        const std::string text = title.empty()
            ? message : title + "\n\n" + message;
        const int count = detail::message_box_button_count(buttons);
        int result;
        if (count == 1) {
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 0), 1,
                nullptr);
        } else if (count == 2) {
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 0), 1,
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 1), 2,
                nullptr);
        } else {
            result = notice_prompt(owner_state->paint_window, nullptr,
                NOTICE_MESSAGE_STRING, text.c_str(),
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 0), 1,
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 1), 2,
                NOTICE_BUTTON,
                detail::message_box_button_label(buttons, 2), 3,
                nullptr);
        }
        return result > 0 && result <= count
            ? detail::message_box_result_for_button(buttons, result - 1)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
