//
// Implements the OpenMotif shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <X11/Shell.h>
#include <algorithm>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace linux::openmotif
{
    XtAppContext app_instance = nullptr;
    bool exit_requested = false;

    native::bindings<Widget, native::wnd *> wnd_bindings;
    native::bindings<Widget, native::wnd *> shell_bindings;
    native::bindings<Widget, native::wnd *> main_wnd_bindings;
    native::bindings<uint32_t, motif_font *> font_bindings;
    native::bindings<uint32_t, motif_menu *> menu_bindings;
    native::bindings<native::file_dialog *, motif_file_dialog *>
        file_dialog_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;

    void set_work_dimensions(native::wnd *owner, native::size dimensions) {
        Widget shell = shell_bindings.handle_from_object(owner);
        Widget canvas = wnd_bindings.handle_from_object(owner);
        if (!shell || !canvas)
            return;

        Dimension shell_width = 0, shell_height = 0;
        Dimension canvas_width = 0, canvas_height = 0;
        XtVaGetValues(shell, XtNwidth, &shell_width,
                      XtNheight, &shell_height, nullptr);
        XtVaGetValues(canvas, XmNwidth, &canvas_width,
                      XmNheight, &canvas_height, nullptr);
        // Use negotiated geometry so CDE font/menu resources determine
        // the reservation, including any main-window margins.
        const int extra_width = std::max(0,
            int(shell_width) - int(canvas_width));
        const int extra_height = std::max(0,
            int(shell_height) - int(canvas_height));
        XtVaSetValues(shell,
                      XtNwidth, dimensions.w + extra_width,
                      XtNheight, dimensions.h + extra_height, nullptr);
    }

    Widget parent_widget(native::wnd *child) {
        native::wnd *parent = child ? child->get_parent() : nullptr;
        if (!parent || !parent->get_created())
            return nullptr;
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *state = tab_view_bindings.object_from_handle(tabs);
            if (!state)
                return nullptr;
            for (std::size_t index = 0;
                 index < tabs->get_item_count() &&
                 index < state->pages.size(); ++index) {
                if (&tabs->get_item(index).get_content() == child)
                    return state->pages[index];
            }
            return nullptr;
        }
        return wnd_bindings.handle_from_object(parent);
    }
} // namespace linux::openmotif
