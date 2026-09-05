//
// Implements portable window geometry, invalidation, and graphics
// access for XView frames and Panel controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/cursorfont.h>

#include <native.h>
#include <native/wnd.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "../../gpx_wnd.h"
#include "globals.h"
#include "collection_host.h"
#include "window_position.h"

namespace
{
    Cursor cursor_for(Display *display, native::mouse_cursor cursor) {
        if (!display)
            return None;

        unsigned int shape = XC_left_ptr;
        if (cursor == native::mouse_cursor::ibeam)
            shape = XC_xterm;
        else if (cursor == native::mouse_cursor::crosshair)
            shape = XC_crosshair;
        else if (cursor == native::mouse_cursor::resize_horizontal)
            shape = XC_sb_h_double_arrow;
        else if (cursor == native::mouse_cursor::resize_vertical)
            shape = XC_sb_v_double_arrow;
        else if (cursor == native::mouse_cursor::resize_northwest_southeast)
            shape = XC_bottom_right_corner;
        else if (cursor == native::mouse_cursor::resize_northeast_southwest)
            shape = XC_bottom_left_corner;
        return XCreateFontCursor(display, shape);
    }

    Panel_item control_item(native::wnd *window) {
        Xv_opaque handle = linux::openlook::wnd_bindings
                               .handle_from_object(window);
        return static_cast<Panel_item>(handle);
    }

    linux::openlook::openlook_collection *collection_state(
        native::wnd *window) {
        return window ? native::detail::peer_state<
                            linux::openlook::openlook_collection>(*window)
                      : nullptr;
    }

    linux::openlook::openlook_combo_box *combo_state(
        native::wnd *window) {
        return window ? native::detail::peer_state<
                            linux::openlook::openlook_combo_box>(*window)
                      : nullptr;
    }

    void position_combo(native::wnd *window,
                        linux::openlook::openlook_combo_box *state) {
        const native::rect bounds = window->get_bounds();
        const bool editable = state->text != XV_NULL;
        const int choice_width = editable
            ? std::min<int>(bounds.d.w, bounds.d.h+6) : bounds.d.w;
        if (state->text)
            xv_set(state->text,
                   XV_X, bounds.p.x,
                   XV_Y, bounds.p.y,
                   PANEL_VALUE_DISPLAY_WIDTH, std::max(1,
                       static_cast<int>(bounds.d.w)-choice_width),
                   XV_WIDTH, std::max(1,
                       static_cast<int>(bounds.d.w)-choice_width),
                   XV_HEIGHT, bounds.d.h,
                   nullptr);
        if (state->choice)
            xv_set(state->choice,
                   XV_X, editable ? bounds.x2()-choice_width : bounds.p.x,
                   XV_Y, bounds.p.y,
                   XV_WIDTH, choice_width,
                   XV_HEIGHT, bounds.d.h,
                   nullptr);
    }

    void fit_list_height(Panel_item item, int requested_height) {
        const int rows = static_cast<int>(xv_get(
            item, PANEL_LIST_DISPLAY_ROWS));
        const int row_height = static_cast<int>(xv_get(
            item, PANEL_LIST_ROW_HEIGHT));
        const int native_height = static_cast<int>(xv_get(
            item, XV_HEIGHT));
        if (rows <= 0 || row_height <= 0)
            return;
        const int chrome = std::max(
            0, native_height - rows * row_height);
        // Rounding up clips the list's own bottom border in a page host.
        // Keep whole native rows, including their surrounding chrome.
        const int fitted_rows = std::max(
            1,
            std::max(0, requested_height - chrome) / row_height);
        if (fitted_rows != rows) {
            xv_set(item,
                   PANEL_LIST_DISPLAY_ROWS,
                   fitted_rows,
                   nullptr);
        }
    }

    native::point frame_position(native::wnd *window,
                                 int &menu_height) {
        native::point result = window->get_position();
        native::wnd *root = window->get_parent();
        while (root &&
               !native::detail::peer_state<
                   linux::openlook::openlook_window>(*root)) {
            result.x = static_cast<native::coord>(
                result.x + root->get_position().x);
            result.y = static_cast<native::coord>(
                result.y + root->get_position().y);
            root = root->get_parent();
        }
        auto *state = root ? native::detail::peer_state<
                                 linux::openlook::openlook_window>(*root)
                           : nullptr;
        menu_height = state ? state->menu_height : 0;
        return result;
    }

} // namespace

namespace native
{
    void wnd::apply_position() {
        if (auto *state = native::detail::peer_state<
                linux::openlook::openlook_window>(*this)) {
            if (!state || !state->frame)
                return;
            const point position =
                linux::openlook::constrain_frame_position(
                    state->frame, _bounds.p, _bounds.d);
            xv_set(state->frame,
                   XV_X,
                   position.x,
                   XV_Y,
                   position.y,
                   nullptr);
            return;
        }

        if (auto *state = collection_state(this)) {
            int menu_height = 0;
            const point position = frame_position(this, menu_height);
            xv_set(state->panel,
                   XV_X,
                   position.x,
                   XV_Y,
                   position.y + menu_height,
                   nullptr);
            if (state->content_panel) {
                const auto *tabs = dynamic_cast<tab_view *>(this);
                const rect content = tabs
                    ? tabs->get_content_bounds()
                    : rect(0, 0, _bounds.d.w, _bounds.d.h);
                xv_set(state->content_panel,
                       XV_X, position.x + content.p.x,
                       XV_Y, position.y + menu_height + content.p.y,
                       nullptr);
            }
            return;
        }
        if (auto *state = combo_state(this)) {
            position_combo(this, state);
            return;
        }

        Panel_item item = control_item(this);
        if (item) {
            int y = _bounds.p.y;
            // XView lists have integral rows. Keep the bottom-facing tab
            // join flush with the list's real border, not its allocation.
            const auto *tabs = dynamic_cast<tab_view *>(get_parent());
            if (dynamic_cast<list *>(this) && tabs &&
                tabs->get_tab_placement() == tab_placement::bottom) {
                y += std::max(0, static_cast<int>(_bounds.d.h) -
                    static_cast<int>(xv_get(item, XV_HEIGHT)));
            }
            xv_set(item,
                   XV_X,
                   _bounds.p.x,
                   XV_Y,
                   y,
                   nullptr);
        }
    }

    void wnd::apply_dimensions() {
        if (auto *state = native::detail::peer_state<
                linux::openlook::openlook_window>(*this)) {
            if (!state || !state->frame)
                return;
            xv_set(state->content,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h,
                   nullptr);
            xv_set(state->frame,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h + state->menu_height,
                   nullptr);
            apply_position();
            return;
        }

        if (auto *state = collection_state(this)) {
            linux::openlook::resize_collection_panel(
                *this, *state, _bounds.d);
            return;
        }
        if (auto *state = combo_state(this)) {
            position_combo(this, state);
            return;
        }

        Panel_item item = control_item(this);
        if (item) {
            if (dynamic_cast<list *>(this)) {
                const auto scrollbar = xv_get(item, PANEL_LIST_SCROLLBAR);
                const int scrollbar_width = scrollbar
                    ? static_cast<int>(xv_get(scrollbar, XV_WIDTH)) : 0;
                xv_set(item, PANEL_LIST_WIDTH,
                    std::max(1, static_cast<int>(_bounds.d.w) -
                        scrollbar_width), nullptr);
                fit_list_height(item, _bounds.d.h);
                apply_position();
                return;
            }
            xv_set(item,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h,
                   nullptr);
            // PANEL_LABEL_WIDTH is a button attribute in the XView
            // implementations we support.  Applying it to check, radio,
            // list, or text items emits a bad-attribute warning and can
            // disturb their native geometry.
            if (dynamic_cast<button *>(this))
                linux::openlook::fit_item_width(item, _bounds.d.w);
        }
    }

    void wnd::apply_bounds() {
        apply_dimensions();
        apply_position();
    }

    void wnd::apply_parent() {
        if (native::detail::peer_state<
                linux::openlook::openlook_window>(*this))
            return;

        Panel_item item = control_item(this);
        const bool was_visible =
            item && static_cast<bool>(xv_get(item, XV_SHOW));
        destroy();
        if (_parent) {
            create();
            if (was_visible)
                show();
        }
    }

    void wnd::apply_cursor() {
        const Window target = linux::openlook::drawable(this);
        Display *display = linux::openlook::cached_display;
        if (!display || target == None)
            return;

        Cursor cursor = cursor_for(display, _cursor);
        if (cursor != None) {
            XDefineCursor(display, target, cursor);
            XFreeCursor(display, cursor);
        }
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;

        auto *self = this;
        if (native::detail::peer_state<
                linux::openlook::openlook_window>(*self)) {
            auto *window = static_cast<app_wnd *>(self);
            linux::openlook::repaint_window(
                window,
                rect(point(0, 0), window->get_dimensions()));
        } else if (collection_state(self)) {
            linux::openlook::repaint_collection(
                *self,
                rect(point(0, 0), self->get_dimensions()));
        } else if (Panel_item item = control_item(self)) {
            panel_paint(item, PANEL_CLEAR);
        }
        return *self;
    }

    wnd &wnd::invalidate_native(const rect &area) {
        if (!_created)
            return *this;

        auto *self = this;
        if (native::detail::peer_state<
                linux::openlook::openlook_window>(*self)) {
            auto *window = static_cast<app_wnd *>(self);
            linux::openlook::repaint_window(window, area);
        } else if (collection_state(self)) {
            linux::openlook::repaint_collection(*self, area);
        } else if (Panel_item item = control_item(self)) {
            panel_paint(item, PANEL_CLEAR);
        }
        return *self;
    }

    gpx &wnd::get_gpx() {
        if (!_created) {
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");
        }
        if (!_gpx)
            _gpx = new gpx_wnd(this);
        return *_gpx;
    }
} // namespace native
