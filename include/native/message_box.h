//
// Declares portable standard message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

namespace native
{
    class app_wnd;

    enum class message_box_buttons
    {
        ok,
        ok_cancel,
        yes_no,
        yes_no_cancel
    };

    enum class message_box_icon
    {
        none,
        information,
        warning,
        error,
        question
    };

    enum class message_box_result
    {
        none,
        ok,
        cancel,
        yes,
        no
    };

    class message_box final
    {
    public:
        // Show an owner-modal standard dialog with one to three buttons.
        // The owner must already be created. Closing the window returns
        // cancel when that button is present, and none otherwise.
        static message_box_result show(
            app_wnd &owner,
            const std::string &message,
            const std::string &title = "Message",
            message_box_buttons buttons = message_box_buttons::ok,
            message_box_icon icon = message_box_icon::none);

    private:
        message_box() = delete;
    };
} // namespace native
