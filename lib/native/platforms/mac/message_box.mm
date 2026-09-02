//
// Implements standard AppKit message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "../../message_box_common.h"

#import <AppKit/AppKit.h>

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:
            [NSString stringWithUTF8String:message.c_str()]];
        switch (icon) {
        case message_box_icon::error:
            [alert setAlertStyle:NSAlertStyleCritical];
            break;
        case message_box_icon::warning:
        case message_box_icon::question:
            [alert setAlertStyle:NSAlertStyleWarning];
            break;
        default:
            [alert setAlertStyle:NSAlertStyleInformational];
            break;
        }
        const int count = detail::message_box_button_count(buttons);
        for (int index = 0; index < count; ++index)
            [alert addButtonWithTitle:[NSString stringWithUTF8String:
                detail::message_box_button_label(buttons, index)]];
        const NSModalResponse response = [alert runModal];
        [alert release];
        const int index = static_cast<int>(response-
            NSAlertFirstButtonReturn);
        return index >= 0 && index < count
            ? detail::message_box_result_for_button(buttons, index)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
