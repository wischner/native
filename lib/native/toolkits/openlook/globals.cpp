//
// Defines XView backend registries and shared native-resource lookup
// helpers without exposing XView types through the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"

#include <algorithm>
#include <stdexcept>

#include <olgx/olgx.h>
#include <xview/panel.h>
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
    native::bindings<native::combo_box *, openlook_combo_box *>
        combo_box_bindings;
    native::bindings<native::accordion *, openlook_collection *>
        accordion_bindings;
    native::bindings<native::tab_view *, openlook_collection *>
        tab_view_bindings;
    native::bindings<native::split_view *, openlook_split_view *>
        split_view_bindings;
    native::bindings<native::icon_view *, openlook_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, openlook_collection *>
        tree_view_bindings;
    native::bindings<native::table_view *, openlook_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, openlook_collection *>
        code_edit_bindings;
    native::bindings<Xv_Window, native::wnd *>
        collection_paint_bindings;
    native::bindings<
        const native::file_dialog *, openlook_file_dialog *>
        file_dialog_bindings;

    native::size menu_mark_dimensions(const void *information) {
        const auto *metrics = static_cast<const Graphics_info *>(
            information);
        if (!metrics)
            return native::size();
        return native::size(
            static_cast<native::dim>(
                std::max(0, static_cast<int>(MenuMark_Width(metrics)))),
            static_cast<native::dim>(
                std::max(0, static_cast<int>(MenuMark_Height(metrics)))));
    }

    Panel parent_panel(native::wnd *control) {
        native::wnd *parent = control ? control->get_parent() : nullptr;
        if (!parent || !parent->get_created()) {
            throw std::runtime_error(
                "OpenLook/XView: control requires a created parent.");
        }
        Xv_opaque handle = XV_NULL;
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *state = tab_view_bindings.object_from_handle(tabs);
            handle = state ? state->content_panel : XV_NULL;
        } else if (auto *split =
                       dynamic_cast<native::split_view *>(parent)) {
            auto *state = split_view_bindings.object_from_handle(split);
            if (state) {
                handle = control == &split->get_second()
                             ? state->second
                             : state->first;
            }
        } else {
            handle = parent
                         ? wnd_bindings.handle_from_object(parent)
                         : XV_NULL;
        }
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

        if (auto *accordion =
                dynamic_cast<native::accordion *>(window)) {
            auto *state = accordion_bindings.object_from_handle(
                accordion);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }
        if (auto *tabs = dynamic_cast<native::tab_view *>(window)) {
            auto *state = tab_view_bindings.object_from_handle(tabs);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }
        if (auto *icons = dynamic_cast<native::icon_view *>(window)) {
            auto *state = icon_view_bindings.object_from_handle(icons);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }
        if (auto *tree = dynamic_cast<native::tree_view *>(window)) {
            auto *state = tree_view_bindings.object_from_handle(tree);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }
        if (auto *table = dynamic_cast<native::table_view *>(window)) {
            auto *state = table_view_bindings.object_from_handle(table);
            return state && state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }
        if (auto *editor = dynamic_cast<native::code_edit *>(window)) {
            auto *state = code_edit_bindings.object_from_handle(editor);
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

    void fit_item_width(Xv_opaque item, native::dim width) {
        if (!item || width == 0)
            return;

        const int target = static_cast<int>(width);
        int label = target;

        // Two passes: the first sets the label and measures what the
        // toolkit's border added, the second takes that back off. The
        // border is a constant, so the second pass lands exactly.
        for (int pass = 0; pass < 2; ++pass) {
            xv_set(item, PANEL_LABEL_WIDTH, label, nullptr);

            const int actual = static_cast<int>(xv_get(item, XV_WIDTH));
            const int overshoot = actual - target;
            if (overshoot <= 0)
                break;

            // Never shrink the label away entirely: a control too
            // narrow for any border is better left slightly wide than
            // rendered as an empty box.
            if (overshoot >= label)
                break;

            label -= overshoot;
        }
    }
} // namespace linux::openlook
