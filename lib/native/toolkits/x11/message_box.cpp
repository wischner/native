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
#include "../../message_box_common.h"

namespace
{
    struct callback_state
    {
        native::message_box_result result =
            native::message_box_result::none;
        bool done = false;
        Atom delete_window = None;
        native::message_box_buttons buttons =
            native::message_box_buttons::ok;
    };

    void choose(Widget, XtPointer data, XtPointer) {
        auto *pair = static_cast<std::pair<callback_state *,
            native::message_box_result> *>(data);
        pair->first->result = pair->second;
        pair->first->done = true;
    }

    void close_shell(Widget,
                     XtPointer data,
                     XEvent *event,
                     Boolean *) {
        auto *state = static_cast<callback_state *>(data);
        if (event->type != ClientMessage ||
            static_cast<Atom>(event->xclient.data.l[0]) !=
                state->delete_window)
            return;
        state->result = native::detail::message_box_dismissed_result(
            state->buttons);
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
        message_box_icon) {
        detail::validate_message_box_owner(owner);
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
        state.buttons = buttons;
        std::pair<callback_state *, message_box_result> callbacks[3];
        const int count = detail::message_box_button_count(buttons);
        for (int index = 0; index < count; ++index) {
            callbacks[index] = {
                &state,
                detail::message_box_result_for_button(buttons, index)};
            XawDialogAddButton(
                dialog,
                detail::message_box_button_label(buttons, index),
                choose,
                &callbacks[index]);
        }
        XtRealizeWidget(shell);
        state.delete_window = XInternAtom(
            XtDisplay(shell), "WM_DELETE_WINDOW", False);
        XSetWMProtocols(XtDisplay(shell), XtWindow(shell),
                        &state.delete_window, 1);
        XtAddEventHandler(shell, NoEventMask, True,
                          close_shell, &state);
        XtPopup(shell, XtGrabExclusive);
        while (!state.done)
            XtAppProcessEvent(linux::x11::app_instance, XtIMAll);
        XtDestroyWidget(shell);
        return state.result;
    }
} // namespace native
