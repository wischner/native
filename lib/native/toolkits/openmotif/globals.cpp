//
// Implements the OpenMotif shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Xm/Xm.h>
#include <X11/Xlib.h>

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
    native::bindings<native::wnd *, motif_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, motif_font *> font_bindings;
    native::bindings<uint32_t, motif_menu *> menu_bindings;
    native::bindings<native::button *, motif_button *> button_bindings;
    native::bindings<native::text_edit *, motif_text_edit *>
        text_edit_bindings;
    native::bindings<native::list *, Widget> list_content_bindings;
    native::bindings<native::combo_box *, Widget> combo_box_bindings;
    native::bindings<native::accordion *, motif_collection *>
        accordion_bindings;
    native::bindings<native::tab_view *, motif_tab_view *>
        tab_view_bindings;
    native::bindings<native::split_view *, motif_split_view *>
        split_view_bindings;
    native::bindings<native::icon_view *, motif_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, motif_collection *>
        tree_view_bindings;
    native::bindings<native::table_view *, motif_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, motif_collection *>
        code_edit_bindings;
    native::bindings<native::canvas *, motif_collection *>
        canvas_bindings;
    native::bindings<native::file_dialog *, motif_file_dialog *>
        file_dialog_bindings;
    Display *cached_display = nullptr;
    Atom wm_delete_window_atom = None;

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
