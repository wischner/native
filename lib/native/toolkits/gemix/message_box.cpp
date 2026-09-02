//
// Implements GEM AES standard alert dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <string>

#include <gem.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "../../message_box_common.h"

namespace
{
    std::string alert_text(std::string value) {
        std::replace(value.begin(), value.end(), '\n', '|');
        std::replace(value.begin(), value.end(), '[', '(');
        std::replace(value.begin(), value.end(), ']', ')');
        return value;
    }
}

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        int symbol = 0;
        switch (icon) {
        case message_box_icon::information: symbol = 1; break;
        case message_box_icon::question:
        case message_box_icon::warning: symbol = 2; break;
        case message_box_icon::error: symbol = 3; break;
        default: break;
        }
        std::string labels;
        const int count = detail::message_box_button_count(buttons);
        for (int index = 0; index < count; ++index) {
            if (!labels.empty()) labels += '|';
            labels += detail::message_box_button_label(buttons, index);
        }
        std::string body = title.empty() ? message : title + "|" + message;
        std::string encoded = "[" + std::to_string(symbol) + "][" +
            alert_text(body) + "][" + labels + "]";
        const int result = form_alert(1,
            const_cast<char *>(encoded.c_str()));
        return result > 0 && result <= count
            ? detail::message_box_result_for_button(buttons, result - 1)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
