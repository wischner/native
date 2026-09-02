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
        app_wnd &,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        int symbol = 0;
        switch (icon) {
        case message_box_icon::information: symbol = 1; break;
        case message_box_icon::question:
        case message_box_icon::warning: symbol = 2; break;
        case message_box_icon::error: symbol = 3; break;
        default: break;
        }
        std::string labels = "OK";
        if (buttons == message_box_buttons::ok_cancel)
            labels = "OK|Cancel";
        else if (buttons == message_box_buttons::yes_no)
            labels = "Yes|No";
        else if (buttons == message_box_buttons::yes_no_cancel)
            labels = "Yes|No|Cancel";
        std::string body = title.empty() ? message : title + "|" + message;
        std::string encoded = "[" + std::to_string(symbol) + "][" +
            alert_text(body) + "][" + labels + "]";
        const int result = form_alert(1,
            const_cast<char *>(encoded.c_str()));
        if (buttons == message_box_buttons::ok)
            return result == 1 ? message_box_result::ok
                               : message_box_result::none;
        if (buttons == message_box_buttons::ok_cancel)
            return result == 1 ? message_box_result::ok
                 : result == 2 ? message_box_result::cancel
                               : message_box_result::none;
        if (buttons == message_box_buttons::yes_no)
            return result == 1 ? message_box_result::yes
                 : result == 2 ? message_box_result::no
                               : message_box_result::none;
        return result == 1 ? message_box_result::yes
             : result == 2 ? message_box_result::no
             : result == 3 ? message_box_result::cancel
                           : message_box_result::none;
    }
} // namespace native
