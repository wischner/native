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
    XtAppContext app_instance = nullptr;
    bool exit_requested = false;
    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<Widget, native::wnd *> shell_bindings;
    native::bindings<Widget, native::wnd *> main_wnd_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;
    native::bindings<native::wnd *, x11_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, x11_font *> font_bindings;
    native::bindings<uint32_t, xaw_menu *> menu_bindings;
    native::bindings<native::button *, xaw_button *> button_bindings;
    native::bindings<native::list *, xaw_list *> list_bindings;
    native::bindings<native::combo_box *, xaw_combo_box *>
        combo_box_bindings;
    native::bindings<native::accordion *, xaw_collection *>
        accordion_bindings;
    native::bindings<native::tab_view *, xaw_collection *>
        tab_view_bindings;
    native::bindings<native::split_view *, xaw_split_view *>
        split_view_bindings;
    native::bindings<native::icon_view *, xaw_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, xaw_collection *>
        tree_view_bindings;
    native::bindings<native::table_view *, xaw_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, xaw_collection *>
        code_edit_bindings;
    native::bindings<native::text_edit *, xaw_text_edit *>
        text_edit_bindings;
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
