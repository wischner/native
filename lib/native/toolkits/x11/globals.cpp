//
// Implements the X11 shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace linux::x11
{
    Widget parent_widget(const native::wnd *child) {
        auto *parent = child ? child->get_parent() : nullptr;
        if (!parent) return nullptr;
        if (auto *split = dynamic_cast<native::split_view *>(parent)) {
            if (auto *state = split_view_bindings.object_from_handle(split))
                return child == &split->get_first() ? state->first : state->second;
        }
        return wnd_bindings.handle_from_object(parent);
    }

    Dimension widget_dimension(int value) {
        return static_cast<Dimension>(value > 0 ? value : 1);
    }

    XtAppContext app_instance = nullptr;
    bool exit_requested = false;
    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<Widget, native::wnd *> shell_bindings;
    native::bindings<Widget, native::wnd *> main_wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
    native::bindings<uint32_t, x11_font *> font_bindings;
    native::bindings<uint32_t, xaw_menu *> menu_bindings;
    native::bindings<
        const native::file_dialog *, xaw_file_dialog *>
        file_dialog_bindings;

    xaw_file_dialog::~xaw_file_dialog() {
        if (!shell)
            return;
        if (XtIsRealized(shell))
            XtPopdown(shell);
        XtDestroyWidget(shell);
    }
} // namespace linux::x11
