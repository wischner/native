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
        auto *owner_state = linux::wmaker::state(&owner);
        if (!owner_state || !owner_state->window)
            throw std::runtime_error(
                "Window Maker/WINGs: Message box has no owner.");

        const int count = detail::message_box_button_count(buttons);
        const char *first =
            detail::message_box_button_label(buttons, 0);
        const char *second = count > 1
            ? detail::message_box_button_label(buttons, 1) : nullptr;
        const char *third = count > 2
            ? detail::message_box_button_label(buttons, 2) : nullptr;
        const int result = WMRunAlertPanel(
            linux::wmaker::screen, owner_state->window,
            title.c_str(), message.c_str(), first, second, third);
        const int index = result == WAPRDefault ? 0
                        : result == WAPRAlternate ? 1
                        : result == WAPROther ? 2 : -1;
        return index >= 0 && index < count
            ? detail::message_box_result_for_button(buttons, index)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
