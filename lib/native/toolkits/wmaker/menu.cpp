//
// Implements application menus with native WINGs pull-down buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdint>

#include <WINGs/WINGs.h>

#include <native/app_wnd.h>
#include <native/menu.h>

#include "globals.h"

namespace
{
    std::uint32_t next_id() {
        static std::uint32_t current = 0;
        return ++current;
    }

    void activate(WMWidget *, void *client_data) {
        auto *callback =
            static_cast<linux::wmaker::menu_callback *>(client_data);
        if (!callback || !callback->owner || !callback->popup ||
            !linux::wmaker::permit_input(callback->owner)) {
            return;
        }
        const int index =
            WMGetPopUpButtonSelectedItem(callback->popup);
        if (index >= 0 &&
            index < static_cast<int>(callback->item_ids.size())) {
            native::app_wnd *owner = callback->owner;
            const int id = callback->item_ids[
                static_cast<std::size_t>(index)];
            linux::wmaker::defer([owner, id]() {
                if (owner->get_created())
                    owner->on_menu.emit(id);
            });
        }
    }
} // namespace

namespace native
{
    main_menu::~main_menu() {
        detach();
    }

    void main_menu::detach() {
        auto *menu = _id
                         ? linux::wmaker::menu_bindings
                               .object_from_handle(_id)
                         : nullptr;
        if (menu) {
            for (WMPopUpButton *popup : menu->popups) {
                if (popup)
                    WMDestroyWidget(popup);
            }
            for (auto *callback : menu->callbacks)
                delete callback;
            delete menu;
        }
        if (_id)
            linux::wmaker::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;
        auto *window_state = linux::wmaker::state(&owner);
        if (!window_state || !window_state->window)
            return;

        auto *menu = new linux::wmaker::native_menu;
        menu->owner = &owner;
        int x = 0;
        constexpr int menu_height = 24;
        WMFont *font = WMDefaultSystemFont(linux::wmaker::screen);

        // Created and mapped first so it sits behind the buttons, and
        // as wide as the window so the strip beside the last button
        // is part of the menu bar rather than bare background.
        menu->background = WMCreateFrame(window_state->window);
        WMSetFrameRelief(menu->background, WRFlat);
        WMMoveWidget(menu->background, 0, 0);
        WMResizeWidget(
            menu->background,
            static_cast<unsigned int>(
                std::max<int>(1, owner.get_dimensions().w)),
            menu_height);
        WMMapWidget(menu->background);
        for (const auto &top : _tops) {
            const int width = std::max(
                52, WMWidthOfString(font,
                                    top.title.c_str(),
                                    static_cast<int>(
                                        top.title.size())) +
                        24);
            WMPopUpButton *popup =
                WMCreatePopUpButton(window_state->window);
            WMSetPopUpButtonPullsDown(popup, True);
            WMSetPopUpButtonText(popup, top.title.c_str());
            WMMoveWidget(popup, x, 0);
            WMResizeWidget(popup,
                           static_cast<unsigned int>(width),
                           menu_height);

            auto *callback = new linux::wmaker::menu_callback;
            callback->owner = &owner;
            callback->popup = popup;
            for (const auto &item : top.items) {
                WMAddPopUpButtonItem(popup, item.label.c_str());
                callback->item_ids.push_back(item.id);
            }
            WMSetPopUpButtonAction(popup, activate, callback);
            WMMapWidget(popup);
            menu->popups.push_back(popup);
            menu->callbacks.push_back(callback);
            x += width;
        }

        _owner = &owner;
        _id = next_id();
        window_state->menu_height = menu_height;
        linux::wmaker::menu_bindings.register_pair(_id, menu);
    }
} // namespace native

namespace linux::wmaker
{
    void resize_menu_bar(native::app_wnd *owner, int width) {
        if (!owner || !owner->menu.id())
            return;

        native_menu *menu =
            menu_bindings.object_from_handle(owner->menu.id());
        if (!menu || !menu->background)
            return;

        WMResizeWidget(menu->background,
                       static_cast<unsigned int>(std::max(1, width)),
                       static_cast<unsigned int>(
                           std::max(1, state(owner)
                                           ? state(owner)->menu_height
                                           : 24)));
    }
} // namespace linux::wmaker
