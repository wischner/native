//
// Implements standard Haiku alert dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Alert.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

namespace native
{
    message_box_result message_box::show(
        app_wnd &,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        const char *first = "OK";
        const char *second = nullptr;
        const char *third = nullptr;
        if (buttons == message_box_buttons::ok_cancel)
            second = "Cancel";
        else if (buttons == message_box_buttons::yes_no) {
            first = "Yes";
            second = "No";
        } else if (buttons == message_box_buttons::yes_no_cancel) {
            first = "Yes";
            second = "No";
            third = "Cancel";
        }

        alert_type type = B_EMPTY_ALERT;
        switch (icon) {
        case message_box_icon::information: type = B_INFO_ALERT; break;
        case message_box_icon::warning:
        case message_box_icon::question: type = B_WARNING_ALERT; break;
        case message_box_icon::error: type = B_STOP_ALERT; break;
        default: break;
        }
        BAlert *alert = new BAlert(title.c_str(),
                                   message.c_str(),
                                   first,
                                   second,
                                   third,
                                   B_WIDTH_AS_USUAL,
                                   type);
        const int32 index = alert->Go();
        if (buttons == message_box_buttons::ok)
            return index == 0 ? message_box_result::ok
                              : message_box_result::none;
        if (buttons == message_box_buttons::ok_cancel)
            return index == 0 ? message_box_result::ok
                 : index == 1 ? message_box_result::cancel
                              : message_box_result::none;
        if (buttons == message_box_buttons::yes_no)
            return index == 0 ? message_box_result::yes
                 : index == 1 ? message_box_result::no
                              : message_box_result::none;
        return index == 0 ? message_box_result::yes
             : index == 1 ? message_box_result::no
             : index == 2 ? message_box_result::cancel
                          : message_box_result::none;
    }
} // namespace native
