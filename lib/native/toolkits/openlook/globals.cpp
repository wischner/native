//
// Defines XView backend registries and shared native-resource lookup
// helpers without exposing XView types through the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"
#include "../../tab_order_binding.h"

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
    native::bindings<std::uint32_t, openlook_font *> font_bindings;
    native::bindings<std::uint32_t, openlook_menu *> menu_bindings;
    native::bindings<Xv_Window, native::wnd *>
        collection_paint_bindings;
    native::bindings<
        const native::file_dialog *, openlook_file_dialog *>
        file_dialog_bindings;

    void dispatch_panel_item(Panel_item item, Event *event) {
        auto *control = wnd_bindings.object_from_handle(item);
        if (!control) {
            panel_default_handle_event(item, event);
            return;
        }
        const native::detail::window_guard guard(*control);
        panel_default_handle_event(item, event);
        if (!guard.alive() || control->get_tab_order())
            return;
        // XView choices draw a focus arrow during mouse preview even when
        // PANEL_ACCEPT_KEYSTROKE is false. Restore the actual caret's
        // indicator, or hide it when there is no ordered caret item.
        const auto panel = static_cast<Panel>(xv_get(item, XV_OWNER));
        const auto caret = xv_get(panel, PANEL_CARET_ITEM);
        const auto *focused = wnd_bindings.object_from_handle(caret);
        const auto frame = xv_get(panel, WIN_FRAME);
        const auto indicator = xv_get(frame, FRAME_FOCUS_WIN);
        if (indicator)
            xv_set(indicator, XV_SHOW, FALSE, nullptr);
        if (focused && focused->get_tab_order())
            xv_set(panel, PANEL_CARET_ITEM, caret, nullptr);
    }

    void bind_tab_focus(native::wnd &control) {
        auto apply = [&control] {
            const auto item = wnd_bindings.handle_from_object(&control);
            if (!item)
                return;
            const bool eligible = control.get_tab_order() > 0;
            xv_set(item, PANEL_ACCEPT_KEYSTROKE, eligible ? TRUE : FALSE,
                   nullptr);
            const auto panel = static_cast<Panel>(xv_get(item, XV_OWNER));
            if (!eligible && xv_get(panel, PANEL_CARET_ITEM) == item)
                panel_advance_caret(panel);
        };
        if (!dynamic_cast<native::combo_box *>(&control))
            xv_set(wnd_bindings.handle_from_object(&control),
                   PANEL_EVENT_PROC, dispatch_panel_item, nullptr);
        native::detail::assign_peer_state(control,
            new native::detail::tab_order_binding{apply});
        apply();
    }

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
        if (!window)
            return None;

        if (auto *state =
                native::detail::peer_state<openlook_window>(*window)) {
            return state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }

        if (auto *state =
                native::detail::peer_state<openlook_collection>(
                    *window)) {
            return state->paint_window
                       ? static_cast<Window>(xv_get(
                             state->paint_window, XV_XID))
                       : None;
        }

        if (auto *split = dynamic_cast<native::split_view *>(window)) {
            auto *state = split_view_bindings.object_from_handle(split);
            return state && state->host
                       ? static_cast<Window>(
                             xv_get(state->host, XV_XID))
                       : None;
        }

        Xv_opaque item = wnd_bindings.handle_from_object(window);
        if (item && dynamic_cast<native::panel *>(window))
            return static_cast<Window>(xv_get(item, XV_XID));
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
