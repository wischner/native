//
// Implements standard Haiku alert dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Alert.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "../../message_box_common.h"
#include "globals.h"

namespace
{
    class centered_alert final : public BAlert
    {
    public:
        centered_alert(BRect owner, const char *title, const char *message,
                       const char *first, const char *second, const char *third, alert_type type)
            : BAlert(title, message, first, second, third, B_WIDTH_AS_USUAL, type), _owner(owner) {}
        void Show() override {
            // Go prepares the native layout before calling Show, so center
            // the final alert dimensions rather than its initial frame.
            CenterIn(_owner);
            BAlert::Show();
        }
    private:
        BRect _owner;
    };
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
        const int count = detail::message_box_button_count(buttons);
        const char *first =
            detail::message_box_button_label(buttons, 0);
        const char *second = count > 1
            ? detail::message_box_button_label(buttons, 1) : nullptr;
        const char *third = count > 2
            ? detail::message_box_button_label(buttons, 2) : nullptr;

        alert_type type = B_EMPTY_ALERT;
        switch (icon) {
        case message_box_icon::information: type = B_INFO_ALERT; break;
        case message_box_icon::warning:
        case message_box_icon::question: type = B_WARNING_ALERT; break;
        case message_box_icon::error: type = B_STOP_ALERT; break;
        default: break;
        }
        auto *window = haiku::wnd_bindings.handle_from_object(&owner);
        BAlert *alert = new centered_alert(window->Frame(), title.c_str(),
                                   message.c_str(),
                                   first,
                                   second,
                                   third,
                                   type);
        const int32 index = alert->Go();
        return index >= 0
            ? detail::message_box_result_for_button(buttons, index)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
