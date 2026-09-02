//
// Implements standard Motif message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <Xm/MessageB.h>
#include <Xm/Xm.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"
#include "../../message_box_common.h"

namespace
{
    struct result_state
    {
        native::message_box_result result =
            native::message_box_result::none;
        native::message_box_buttons buttons =
            native::message_box_buttons::ok;
        bool done = false;
        bool destroyed = false;
    };

    void accept(Widget, XtPointer data, XtPointer) {
        auto *state = static_cast<result_state *>(data);
        state->result = native::detail::message_box_result_for_button(
            state->buttons, 0);
        state->done = true;
    }

    void reject(Widget, XtPointer data, XtPointer) {
        auto *state = static_cast<result_state *>(data);
        state->result = native::detail::message_box_result_for_button(
            state->buttons, 1);
        state->done = true;
    }

    void auxiliary(Widget, XtPointer data, XtPointer) {
        auto *state = static_cast<result_state *>(data);
        state->result = native::detail::message_box_result_for_button(
            state->buttons, 2);
        state->done = true;
    }

    void destroyed(Widget, XtPointer data, XtPointer) {
        auto *state = static_cast<result_state *>(data);
        if (!state->done)
            state->result =
                native::detail::message_box_dismissed_result(
                    state->buttons);
        state->destroyed = true;
        state->done = true;
    }
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
        Widget parent =
            linux::openmotif::shell_bindings.handle_from_object(&owner);
        if (!parent)
            throw std::runtime_error(
                "Motif: Message dialog has no owner shell.");

        XmString text = XmStringCreateLocalized(
            const_cast<char *>(message.c_str()));
        XmString caption = XmStringCreateLocalized(
            const_cast<char *>(title.c_str()));
        Arg arguments[4];
        Cardinal count = 0;
        XtSetArg(arguments[count], XmNdialogStyle,
                 XmDIALOG_PRIMARY_APPLICATION_MODAL); ++count;
        XtSetArg(arguments[count], XmNmessageString, text); ++count;
        XtSetArg(arguments[count], XmNdialogTitle, caption); ++count;

        Widget dialog = nullptr;
        switch (icon) {
        case message_box_icon::error:
            dialog = XmCreateErrorDialog(parent,
                const_cast<char *>("message_box"), arguments, count);
            break;
        case message_box_icon::warning:
            dialog = XmCreateWarningDialog(parent,
                const_cast<char *>("message_box"), arguments, count);
            break;
        case message_box_icon::question:
            dialog = XmCreateQuestionDialog(parent,
                const_cast<char *>("message_box"), arguments, count);
            break;
        default:
            dialog = XmCreateInformationDialog(parent,
                const_cast<char *>("message_box"), arguments, count);
            break;
        }
        XmStringFree(caption);
        XmStringFree(text);
        if (!dialog)
            throw std::runtime_error(
                "Motif: Failed to create a message dialog.");

        Widget help = XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
        result_state state;
        state.buttons = buttons;
        if (buttons == message_box_buttons::ok) {
            Widget cancel = XmMessageBoxGetChild(
                dialog, XmDIALOG_CANCEL_BUTTON);
            if (cancel) XtUnmanageChild(cancel);
            if (help) XtUnmanageChild(help);
        } else if (buttons == message_box_buttons::ok_cancel) {
            if (help) XtUnmanageChild(help);
        } else {
            XmString yes = XmStringCreateLocalized(const_cast<char *>(
                detail::message_box_button_label(buttons, 0)));
            XmString no = XmStringCreateLocalized(const_cast<char *>(
                detail::message_box_button_label(buttons, 1)));
            XtVaSetValues(dialog,
                          XmNokLabelString, yes,
                          XmNcancelLabelString, no,
                          nullptr);
            XmStringFree(no);
            XmStringFree(yes);
            if (buttons == message_box_buttons::yes_no) {
                if (help) XtUnmanageChild(help);
            } else if (help) {
                XmString cancel = XmStringCreateLocalized(
                    const_cast<char *>(
                        detail::message_box_button_label(buttons, 2)));
                XtVaSetValues(help, XmNlabelString, cancel, nullptr);
                XmStringFree(cancel);
            }
        }
        XtAddCallback(dialog, XmNokCallback, accept, &state);
        XtAddCallback(dialog, XmNcancelCallback, reject, &state);
        if (help)
            XtAddCallback(dialog, XmNhelpCallback, auxiliary, &state);
        XtAddCallback(dialog, XmNdestroyCallback, destroyed, &state);
        XtManageChild(dialog);
        while (!state.done)
            XtAppProcessEvent(linux::openmotif::app_instance,
                              XtIMAll);
        if (!state.destroyed)
            XtDestroyWidget(dialog);
        return state.result;
    }
} // namespace native
