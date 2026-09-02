//
// Implements portable menu bars with native XView Panel buttons and
// OPEN LOOK command menus.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdint>

#include <native.h>
#include <native/menu.h>

#include <xview/frame.h>
#include <xview/openmenu.h>
#include <xview/panel.h>
#include <xview/window.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    std::uint32_t next_id() {
        static std::uint32_t current = 0;
        return ++current;
    }

    Xv_opaque activate(Menu, Menu_item item) {
        auto *callback = reinterpret_cast<
            linux::openlook::openlook_menu_callback *>(
                xv_get(item, MENU_CLIENT_DATA));
        if (callback &&
            linux::openlook::permit_input(callback->owner)) {
            callback->owner->on_native_menu(callback->item_id);
        }
        return item;
    }

    void open_menu(Panel_item, Event *) {
    }
} // namespace

namespace native
{
    main_menu::~main_menu() {
        detach();
    }

    void main_menu::detach() {
        if (!_id) {
            _owner = nullptr;
            return;
        }

        auto *state = linux::openlook::menu_bindings
                          .object_from_handle(_id);
        if (state) {
            if (state->bar)
                xv_destroy_safe(state->bar);
            for (Menu menu : state->menus) {
                if (menu)
                    xv_destroy_safe(menu);
            }
            for (auto *callback : state->callbacks)
                delete callback;
            delete state;
        }
        linux::openlook::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;

        auto *window = linux::openlook::window_state(&owner);
        if (!window || !window->frame)
            return;

        Panel bar = static_cast<Panel>(xv_create(
            window->frame,
            PANEL,
            PANEL_LAYOUT,
            PANEL_HORIZONTAL,
            PANEL_BORDER,
            FALSE,
            XV_X,
            0,
            XV_Y,
            0,
            XV_WIDTH,
            WIN_EXTEND_TO_EDGE,
            nullptr));
        if (!bar)
            return;

        auto *state = new linux::openlook::openlook_menu;
        state->bar = bar;
        state->owner = &owner;

        for (const auto &top : _tops) {
            Menu menu = static_cast<Menu>(xv_create(
                XV_NULL, MENU_COMMAND_MENU, nullptr));
            state->menus.push_back(menu);
            Menu_item previous = static_cast<Menu_item>(XV_NULL);
            for (const auto &entry : top.items) {
                if (entry.separator) {
                    if (previous)
                        xv_set(previous, MENU_LINE_AFTER_ITEM,
                               MENU_HORIZONTAL_LINE, nullptr);
                    continue;
                }
                auto *callback =
                    new linux::openlook::openlook_menu_callback{
                        &owner, entry.id};
                state->callbacks.push_back(callback);
                Menu_item item = static_cast<Menu_item>(xv_create(
                    XV_NULL,
                    MENUITEM,
                    MENU_STRING,
                    entry.label.c_str(),
                    MENU_NOTIFY_PROC,
                    activate,
                    MENU_CLIENT_DATA,
                    callback,
                    nullptr));
                xv_set(menu, MENU_APPEND_ITEM, item, nullptr);
                previous = item;
            }
            xv_create(bar,
                      PANEL_BUTTON,
                      PANEL_LABEL_STRING,
                      top.title.c_str(),
                      PANEL_NOTIFY_PROC,
                      open_menu,
                      PANEL_ITEM_MENU,
                      menu,
                      nullptr);
        }
        window_fit_height(bar);
        window->menu_height = static_cast<int>(xv_get(
            bar, XV_HEIGHT));

        _owner = &owner;
        _id = next_id();
        linux::openlook::menu_bindings.register_pair(_id, state);
    }
} // namespace native
