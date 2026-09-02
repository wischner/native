//
// Implements standard WINGs alert panels.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

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
        message_box_icon) {
        auto *owner_state = linux::wmaker::state(&owner);
        if (!owner_state || !owner_state->window)
            throw std::runtime_error(
                "Window Maker/WINGs: Message box has no owner.");

        const char *first = "OK";
        const char *second = nullptr;
        const char *third = nullptr;
        if (buttons == message_box_buttons::ok_cancel)
            second = "Cancel";
        else if (buttons == message_box_buttons::yes_no) {
            first = "Yes"; second = "No";
        } else if (buttons == message_box_buttons::yes_no_cancel) {
            first = "Yes"; second = "No"; third = "Cancel";
        }
        const int result = WMRunAlertPanel(
            linux::wmaker::screen, owner_state->window,
            title.c_str(), message.c_str(), first, second, third);
        if (buttons == message_box_buttons::ok)
            return result == WAPRDefault ? message_box_result::ok
                                         : message_box_result::none;
        if (buttons == message_box_buttons::ok_cancel)
            return result == WAPRDefault ? message_box_result::ok
                 : result == WAPRAlternate ? message_box_result::cancel
                                           : message_box_result::none;
        if (buttons == message_box_buttons::yes_no)
            return result == WAPRDefault ? message_box_result::yes
                 : result == WAPRAlternate ? message_box_result::no
                                           : message_box_result::none;
        return result == WAPRDefault ? message_box_result::yes
             : result == WAPRAlternate ? message_box_result::no
             : result == WAPROther ? message_box_result::cancel
                                   : message_box_result::none;
    }
} // namespace native
