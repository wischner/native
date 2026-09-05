//
// Implements Athena standard message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <algorithm>
#include <utility>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"
#include "alert_icons.h"
#include "../../message_box_common.h"

namespace
{
    // Use the owner's current screen position, not its creation-time bounds.
    // A hidden owner falls back to the X screen center. Position the shell
    // before mapping so the window manager never first shows it at (0, 0).
    void center_shell(Widget shell, Widget parent) {
        Screen *screen = XtScreen(shell);
        const int screen_width = WidthOfScreen(screen);
        const int screen_height = HeightOfScreen(screen);
        int center_x = screen_width / 2;
        int center_y = screen_height / 2;
        XWindowAttributes attributes{};
        if (XtIsRealized(parent) && XGetWindowAttributes(XtDisplay(parent),
                XtWindow(parent), &attributes) && attributes.map_state == IsViewable) {
            int x = 0, y = 0;
            Window child = None;
            if (XTranslateCoordinates(XtDisplay(parent), XtWindow(parent),
                    RootWindowOfScreen(screen), 0, 0, &x, &y, &child)) {
                center_x = x + attributes.width / 2;
                center_y = y + attributes.height / 2;
            }
        }
        Dimension width = 0, height = 0, border = 0;
        XtVaGetValues(shell, XtNwidth, &width, XtNheight, &height,
            XtNborderWidth, &border, nullptr);
        const int outer_width = width + 2 * border;
        const int outer_height = height + 2 * border;
        XtVaSetValues(shell,
            XtNx, std::clamp(center_x - outer_width / 2,
                0, std::max(0, screen_width - outer_width)),
            XtNy, std::clamp(center_y - outer_height / 2,
                0, std::max(0, screen_height - outer_height)),
            XtNwinGravity, CenterGravity, nullptr);
    }

    void center_buttons(Widget dialog, XtPointer, XEvent *event, Boolean *) {
        if (event && event->type != ConfigureNotify) return;
        WidgetList children = nullptr;
        Cardinal count = 0;
        Dimension width = 1;
        int gap = 4;
        XtVaGetValues(dialog, XtNchildren, &children, XtNnumChildren, &count,
            XtNwidth, &width, XtNdefaultDistance, &gap, nullptr);
        Widget first = nullptr;
        int total = 0;
        for (Cardinal index = 0; index < count; ++index) {
            if (!XtIsSubclass(children[index], commandWidgetClass)) continue;
            Dimension extent = 0, border = 0;
            XtVaGetValues(children[index], XtNwidth, &extent,
                XtNborderWidth, &border, nullptr);
            if (first) total += gap;
            else first = children[index];
            total += extent + 2 * border;
        }
        if (!first) return;
        int current = 0;
        XtVaGetValues(first, XtNhorizDistance, &current, nullptr);
        const int margin = std::max(gap, (int(width) - total) / 2);
        if (current != margin) {
            // Dialog subclasses Form, whose constraint-only updates defer
            // layout. Include the position request to commit that layout.
            XtVaSetValues(first, XtNhorizDistance, margin,
                XtNx, margin, nullptr);
        }
    }

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
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        Widget parent = linux::x11::shell_bindings
                            .handle_from_object(&owner);
        if (!parent)
            throw std::runtime_error(
                "X11: Message dialog has no owner shell.");
        Widget shell = XtVaCreatePopupShell(
            "message_box", transientShellWidgetClass, parent,
            XtNtitle, title.c_str(), nullptr);
        const Pixmap bitmap = linux::x11::create_message_icon(
            XtDisplay(shell), RootWindowOfScreen(XtScreen(shell)), icon);
        Widget dialog = XtVaCreateManagedWidget(
            "message", dialogWidgetClass, shell,
            XtNlabel, message.c_str(), XtNicon, bitmap, nullptr);

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
        center_buttons(dialog, nullptr, nullptr, nullptr);
        center_shell(shell, parent);
        XtAddEventHandler(dialog, StructureNotifyMask, False,
            center_buttons, nullptr);
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
        if (bitmap != None) XFreePixmap(linux::x11::cached_display, bitmap);
        return state.result;
    }
} // namespace native
