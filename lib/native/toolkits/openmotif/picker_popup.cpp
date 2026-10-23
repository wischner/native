//
// Gives Motif calendar popups conventional outside-click dismissal.
// Xt still dispatches inside events to the native calendar buttons;
// the pointer grab also observes clicks outside this application's
// windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../combo_picker.h"
#include <X11/StringDefs.h>

namespace
{
    using namespace linux::openmotif;

    // Restore the original Xt dispatcher before releasing popup
    // resources.
    void release_popup() {
        if (!picker_popup.field)
            return;
        const auto previous = picker_popup.previous;
        picker_popup = {};
        XtSetEventDispatcher(cached_display, ButtonPress, previous);
        XUngrabPointer(cached_display, CurrentTime);
    }

    void popup_destroyed(Widget shell, XtPointer, XtPointer) {
        if (picker_popup.shell == shell)
            release_popup();
    }

    Boolean dispatch_press(XEvent *event) {
        const auto previous = picker_popup.previous;
        const auto shell = picker_popup.shell;
        if (shell && XtIsRealized(shell)) {
            Position x = 0, y = 0;
            Dimension width = 0, height = 0;
            XtTranslateCoords(shell, 0, 0, &x, &y);
            XtVaGetValues(
                shell, XtNwidth, &width, XtNheight, &height, nullptr);
            if (event->xbutton.x_root < x ||
                event->xbutton.x_root >= x + width ||
                event->xbutton.y_root < y ||
                event->xbutton.y_root >= y + height) {
                auto *field = dynamic_cast<native::combo_box *>(
                    wnd_bindings.object_from_handle(
                        picker_popup.field));
                auto *owner = field
                                  ? dynamic_cast<native::date_picker *>(
                                        field->get_parent())
                                  : nullptr;
                auto *calendar =
                    owner ? native::detail::picker_calendar(*owner)
                          : nullptr;
                release_popup();
                if (calendar && calendar->get_created())
                    calendar->destroy();
                // Consume dismissal: its event can name the closed
                // shell.
                return True;
            }
        }
        return previous ? previous(event) : False;
    }
} // namespace

namespace linux::openmotif
{
    void track_picker_popup(native::combo_box &field) {
        auto *owner =
            dynamic_cast<native::date_picker *>(field.get_parent());
        if (!owner)
            return;
        auto *calendar = native::detail::picker_calendar(*owner);
        if (!calendar || !calendar->get_created())
            return;
        Widget shell = shell_bindings.handle_from_object(calendar);
        if (!shell || !XtIsRealized(shell))
            return;
        // Give the override-redirect calendar its own server-drawn
        // border; the date field retains the ordinary XmComboBox frame.
        XtVaSetValues(shell,
                      XtNborderWidth,
                      1,
                      XtNborderColor,
                      BlackPixelOfScreen(XtScreen(shell)),
                      nullptr);
        release_popup();
        picker_popup.field = wnd_bindings.handle_from_object(&field);
        picker_popup.shell = shell;
        picker_popup.previous = XtSetEventDispatcher(
            cached_display, ButtonPress, dispatch_press);
        XtAddCallback(
            shell, XtNdestroyCallback, popup_destroyed, nullptr);
        XGrabPointer(cached_display,
                     XtWindow(shell),
                     True,
                     ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask,
                     GrabModeAsync,
                     GrabModeAsync,
                     None,
                     None,
                     CurrentTime);
    }
} // namespace linux::openmotif
