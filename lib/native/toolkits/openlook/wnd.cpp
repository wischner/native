//
// Implements portable window geometry, invalidation, and graphics
// access for XView frames and Panel controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

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
    Panel_item control_item(native::wnd *window) {
        Xv_opaque handle = linux::openlook::wnd_bindings
                               .handle_from_object(window);
        return static_cast<Panel_item>(handle);
    }

    linux::openlook::openlook_collection *collection_state(
        native::wnd *window) {
        if (auto *accordion =
                dynamic_cast<native::accordion *>(window)) {
            return linux::openlook::accordion_bindings
                .object_from_handle(accordion);
        }
        if (auto *icons = dynamic_cast<native::icon_view *>(window)) {
            return linux::openlook::icon_view_bindings
                .object_from_handle(icons);
        }
        if (auto *tree = dynamic_cast<native::tree_view *>(window)) {
            return linux::openlook::tree_view_bindings
                .object_from_handle(tree);
        }
        if (auto *table = dynamic_cast<native::table_view *>(window)) {
            return linux::openlook::table_view_bindings
                .object_from_handle(table);
        }
        if (auto *editor = dynamic_cast<native::code_edit *>(window)) {
            return linux::openlook::code_edit_bindings
                .object_from_handle(editor);
        }
        return nullptr;
    }

    linux::openlook::openlook_combo_box *combo_state(
        native::wnd *window) {
        auto *combo = dynamic_cast<native::combo_box *>(window);
        return combo ? linux::openlook::combo_box_bindings
                           .object_from_handle(combo) : nullptr;
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

    native::point frame_position(native::wnd *window,
                                 int &menu_height) {
        native::point result = window->get_position();
        native::wnd *root = window->get_parent();
        while (root && !dynamic_cast<native::app_wnd *>(root)) {
            result.x = static_cast<native::coord>(
                result.x + root->get_position().x);
            result.y = static_cast<native::coord>(
                result.y + root->get_position().y);
            root = root->get_parent();
        }
        auto *top = dynamic_cast<native::app_wnd *>(root);
        auto *state = top ? linux::openlook::window_state(top) : nullptr;
        menu_height = state ? state->menu_height : 0;
        return result;
    }

} // namespace

namespace native
{
    void wnd::apply_position() {
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *state = linux::openlook::window_state(window);
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
            return;
        }
        if (auto *state = combo_state(this)) {
            position_combo(this, state);
            return;
        }

        Panel_item item = control_item(this);
        if (item) {
            xv_set(item,
                   XV_X,
                   _bounds.p.x,
                   XV_Y,
                   _bounds.p.y,
                   nullptr);
        }
    }

    void wnd::apply_dimensions() {
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *state = linux::openlook::window_state(window);
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
        if (dynamic_cast<app_wnd *>(this))
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

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
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

    wnd &wnd::invalidate(const rect &area) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
            linux::openlook::repaint_window(window, area);
        } else if (collection_state(self)) {
            linux::openlook::repaint_collection(*self, area);
        } else if (Panel_item item = control_item(self)) {
            panel_paint(item, PANEL_CLEAR);
        }
        return *self;
    }

    gpx &wnd::get_gpx() const {
        if (!_created) {
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");
        }
        if (!_gpx)
            _gpx = new gpx_wnd(this);
        return *_gpx;
    }
} // namespace native
