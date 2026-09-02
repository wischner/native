//
// Defines the shared button contract for standard message boxes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <stdexcept>

#include <native/app_wnd.h>
#include <native/message_box.h>

namespace native::detail
{
    inline void validate_message_box_owner(const app_wnd &owner) {
        if (!owner.get_created())
            throw std::logic_error(
                "A message box requires a created owner.");
    }

    constexpr int message_box_button_count(
        message_box_buttons buttons) {
        return buttons == message_box_buttons::ok ? 1
             : buttons == message_box_buttons::yes_no_cancel ? 3
                                                             : 2;
    }

    constexpr const char *message_box_button_label(
        message_box_buttons buttons,
        int index) {
        switch (buttons) {
        case message_box_buttons::ok:
            return index == 0 ? "OK" : nullptr;
        case message_box_buttons::ok_cancel:
            return index == 0 ? "OK"
                 : index == 1 ? "Cancel" : nullptr;
        case message_box_buttons::yes_no:
            return index == 0 ? "Yes"
                 : index == 1 ? "No" : nullptr;
        case message_box_buttons::yes_no_cancel:
            return index == 0 ? "Yes"
                 : index == 1 ? "No"
                 : index == 2 ? "Cancel" : nullptr;
        }
        return nullptr;
    }

    constexpr message_box_result message_box_result_for_button(
        message_box_buttons buttons,
        int index) {
        switch (buttons) {
        case message_box_buttons::ok:
            return index == 0 ? message_box_result::ok
                              : message_box_result::none;
        case message_box_buttons::ok_cancel:
            return index == 0 ? message_box_result::ok
                 : index == 1 ? message_box_result::cancel
                              : message_box_result::none;
        case message_box_buttons::yes_no:
            return index == 0 ? message_box_result::yes
                 : index == 1 ? message_box_result::no
                              : message_box_result::none;
        case message_box_buttons::yes_no_cancel:
            return index == 0 ? message_box_result::yes
                 : index == 1 ? message_box_result::no
                 : index == 2 ? message_box_result::cancel
                              : message_box_result::none;
        }
        return message_box_result::none;
    }

    constexpr message_box_result message_box_dismissed_result(
        message_box_buttons buttons) {
        return buttons == message_box_buttons::ok_cancel ||
                       buttons == message_box_buttons::yes_no_cancel
                   ? message_box_result::cancel
                   : message_box_result::none;
    }
} // namespace native::detail
