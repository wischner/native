//
// Implements the portable list with the native XView Panel list item.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native.h>
#include <native/list.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    int notify(Panel_item item,
               char *,
               Xv_opaque,
               Panel_list_op operation,
               Event *,
               int row) {
        auto *owner = reinterpret_cast<native::list *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (!linux::openlook::permit_input(owner)) {
            if (owner) {
                const int count = static_cast<int>(
                    owner->get_items().size());
                for (int index = 0; index < count; ++index) {
                    xv_set(item,
                           PANEL_LIST_SELECT,
                           index,
                           index == owner->get_selected_index()
                               ? TRUE
                               : FALSE,
                           nullptr);
                }
            }
        } else if (operation == PANEL_LIST_OP_SELECT) {
            owner->on_native_selection(row);
        }
        return XV_OK;
    }

    Panel_item item_for(native::list *owner) {
        return static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(owner));
    }

    void replace_items(Panel_item item,
                       const std::vector<std::string> &items) {
        const int count = static_cast<int>(xv_get(
            item, PANEL_LIST_NROWS));
        if (count > 0) {
            xv_set(item,
                   PANEL_LIST_DELETE_ROWS,
                   0,
                   count,
                   nullptr);
        }
        for (std::size_t index = 0; index < items.size(); ++index) {
            xv_set(item,
                   PANEL_LIST_INSERT,
                   static_cast<int>(index),
                   PANEL_LIST_STRING,
                   static_cast<int>(index),
                   items[index].c_str(),
                   nullptr);
        }
    }
} // namespace

namespace native
{
    void list::apply_items() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing list binding.");
        }
        replace_items(item, _items);
        apply_selected_index();
    }

    void list::apply_selected_index() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing list binding.");
        }
        const int count = static_cast<int>(_items.size());
        for (int index = 0; index < count; ++index) {
            xv_set(item,
                   PANEL_LIST_SELECT,
                   index,
                   index == _selected_index ? TRUE : FALSE,
                   nullptr);
        }
    }

    void list::create_native() {
        auto *self = this;
        Panel panel = linux::openlook::parent_panel(self);
        const int rows = std::max(
            1, static_cast<int>(_bounds.d.h) / 20);
        Panel_item item = static_cast<Panel_item>(xv_create(
            panel,
            PANEL_LIST,
            PANEL_CHOOSE_ONE,
            TRUE,
            PANEL_CHOOSE_NONE,
            TRUE,
            PANEL_LIST_DISPLAY_ROWS,
            rows,
            PANEL_LIST_WIDTH,
            _bounds.d.w,
            PANEL_NOTIFY_PROC,
            notify,
            PANEL_CLIENT_DATA,
            self,
            XV_X,
            _bounds.p.x,
            XV_Y,
            _bounds.p.y,
            XV_WIDTH,
            _bounds.d.w,
            XV_HEIGHT,
            _bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create list.");
        }
        // PANEL_LIST sizes itself in complete native rows and treats
        // XV_HEIGHT as advisory.  Derive its non-row chrome from the first
        // layout, then choose the closest row count so tall lists fill their
        // requested pane instead of leaving a conspicuous strip below them.
        const int row_height = static_cast<int>(xv_get(
            item, PANEL_LIST_ROW_HEIGHT));
        const int native_height = static_cast<int>(xv_get(
            item, XV_HEIGHT));
        if (row_height > 0) {
            const int chrome = std::max(
                0, native_height - rows * row_height);
            const int fitted_rows = std::max(
                1,
                (std::max(0, static_cast<int>(_bounds.d.h) - chrome) +
                 row_height / 2) /
                    row_height);
            if (fitted_rows != rows) {
                xv_set(item,
                       PANEL_LIST_DISPLAY_ROWS,
                       fitted_rows,
                       nullptr);
            }
        }
        linux::openlook::wnd_bindings.register_pair(item, self);
        replace_items(item, _items);
        if (_selected_index >= 0) {
            xv_set(item,
                   PANEL_LIST_SELECT,
                   _selected_index,
                   TRUE,
                   nullptr);
        }
    }

    void list::show_native() {
        Panel_item item = item_for(this);
        if (!_created || !item) {
            throw std::runtime_error(
                "OpenLook/XView: list is not created.");
        }
        xv_set(item, XV_SHOW, TRUE, nullptr);
    }

    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        Panel_item item = item_for(self);
        if (item) {
            linux::openlook::wnd_bindings.unregister_by_handle(item);
            xv_destroy_safe(item);
        }
    }
} // namespace native
