//
// Implements standard AppKit message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app_wnd.h>
#include <native/message_box.h>

#import <AppKit/AppKit.h>

namespace
{
    NSString *button_title(native::message_box_buttons buttons,
                           int index) {
        switch (buttons) {
        case native::message_box_buttons::ok:
            return index == 0 ? @"OK" : nil;
        case native::message_box_buttons::ok_cancel:
            return index == 0 ? @"OK" : index == 1 ? @"Cancel" : nil;
        case native::message_box_buttons::yes_no:
            return index == 0 ? @"Yes" : index == 1 ? @"No" : nil;
        case native::message_box_buttons::yes_no_cancel:
            return index == 0 ? @"Yes"
                 : index == 1 ? @"No"
                              : index == 2 ? @"Cancel" : nil;
        }
        return nil;
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
        int count = buttons == message_box_buttons::ok ? 1
                  : buttons == message_box_buttons::yes_no_cancel ? 3 : 2;
        for (int index = 0; index < count; ++index)
            [alert addButtonWithTitle:button_title(buttons, index)];
        const NSModalResponse response = [alert runModal];
        [alert release];
        const int index = static_cast<int>(response-
            NSAlertFirstButtonReturn);
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
