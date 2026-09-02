//
// Implements Athena standard message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"

namespace
{
    struct callback_state
    {
        native::message_box_result result =
            native::message_box_result::none;
        bool done = false;
    };

    void choose(Widget, XtPointer data, XtPointer) {
        auto *pair = static_cast<std::pair<callback_state *,
            native::message_box_result> *>(data);
        pair->first->result = pair->second;
        pair->first->done = true;
    }
}

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon) {
        Widget parent = linux::x11::shell_bindings
                            .handle_from_object(&owner);
        if (!parent)
            throw std::runtime_error(
                "X11: Message dialog has no owner shell.");
        Widget shell = XtVaCreatePopupShell(
            "message_box", transientShellWidgetClass, parent,
            XtNtitle, title.c_str(), nullptr);
        Widget dialog = XtVaCreateManagedWidget(
            "message", dialogWidgetClass, shell,
            XtNlabel, message.c_str(), nullptr);

        callback_state state;
        std::pair<callback_state *, message_box_result> callbacks[3];
        int count = 0;
        auto add = [&](const char *label, message_box_result result) {
            callbacks[count] = {&state, result};
            XawDialogAddButton(dialog, label, choose, &callbacks[count]);
            ++count;
        };
        switch (buttons) {
        case message_box_buttons::ok:
            add("OK", message_box_result::ok); break;
        case message_box_buttons::ok_cancel:
            add("OK", message_box_result::ok);
            add("Cancel", message_box_result::cancel); break;
        case message_box_buttons::yes_no:
            add("Yes", message_box_result::yes);
            add("No", message_box_result::no); break;
        case message_box_buttons::yes_no_cancel:
            add("Yes", message_box_result::yes);
            add("No", message_box_result::no);
            add("Cancel", message_box_result::cancel); break;
        }
        XtRealizeWidget(shell);
        XtPopup(shell, XtGrabExclusive);
        while (!state.done)
            XtAppProcessEvent(linux::x11::app_instance, XtIMAll);
        XtDestroyWidget(shell);
        return state.result;
    }
} // namespace native
