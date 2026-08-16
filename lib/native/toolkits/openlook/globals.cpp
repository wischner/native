//
// Defines XView backend registries and shared native-resource lookup
// helpers without exposing XView types through the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"

#include <stdexcept>

#include <xview/window.h>
#include <xview/xview.h>

namespace linux::openlook
{
    bool initialized = false;
    bool exit_requested = false;
    Display *cached_display = nullptr;
    Frame main_frame = XV_NULL;

    native::bindings<Xv_opaque, native::wnd *> wnd_bindings;
    native::bindings<Xv_opaque, native::app_wnd *> frame_bindings;
    native::bindings<native::app_wnd *, openlook_window *>
        window_bindings;
    native::bindings<native::wnd *, openlook_gpx *>
        wnd_gpx_bindings;
    native::bindings<std::uint32_t, openlook_font *> font_bindings;
    native::bindings<std::uint32_t, openlook_menu *> menu_bindings;
    native::bindings<native::text_edit *, openlook_text_edit *>
        text_edit_bindings;
    native::bindings<
        const native::file_dialog *, openlook_file_dialog *>
        file_dialog_bindings;

    Panel parent_panel(native::wnd *control) {
        native::wnd *parent = control ? control->get_parent() : nullptr;
        if (!parent || !parent->get_created()) {
            throw std::runtime_error(
                "OpenLook/XView: control requires a created parent.");
        }
        Xv_opaque handle = wnd_bindings.handle_from_object(parent);
        if (!handle) {
            throw std::runtime_error(
                "OpenLook/XView: parent has no content panel.");
        }
        return static_cast<Panel>(handle);
    }

    openlook_window *window_state(native::app_wnd *window) {
        return window_bindings.object_from_handle(window);
    }

    bool permit_input(native::wnd *window) {
        if (!window)
            return false;
        if (window->get_input_enabled())
            return true;

        native::wnd *root = window;
        while (root->get_parent())
            root = root->get_parent();
        auto *branch = dynamic_cast<native::app_wnd *>(root);
        if (!branch)
            return false;
        native::modal_wnd *modal = branch->get_active_modal();
        while (!modal && branch->get_owner()) {
            native::app_wnd *owner = branch->get_owner();
            native::modal_wnd *candidate =
                owner->get_active_modal();
            if (candidate && candidate != branch)
                modal = candidate;
            branch = owner;
        }
        if (!modal)
            return false;

        if (auto *state = window_state(modal)) {
            if (state->frame) {
                xv_set(state->frame,
                       WIN_FRONT,
                       WIN_SET_FOCUS,
                       nullptr);
            }
            return false;
        }

        auto *dialog = dynamic_cast<native::file_dialog *>(modal);
        auto *state = dialog
                          ? file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (state && state->chooser) {
            xv_set(state->chooser,
                   WIN_FRONT,
                   WIN_SET_FOCUS,
                   nullptr);
        }
        return false;
    }

    Window drawable(native::wnd *window) {
        if (auto *top = dynamic_cast<native::app_wnd *>(window)) {
            openlook_window *state = window_state(top);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }

        Xv_opaque item = wnd_bindings.handle_from_object(window);
        Xv_Window paint_window = item
                                     ? static_cast<Xv_Window>(xv_get(
                                           item,
                                           PANEL_ITEM_NTH_WINDOW,
                                           0))
                                     : XV_NULL;
        return paint_window
                   ? static_cast<Window>(
                         xv_get(paint_window, XV_XID))
                   : None;
    }
} // namespace linux::openlook
