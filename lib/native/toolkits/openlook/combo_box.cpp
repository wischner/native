//
// Implements the native OpenLook choice/text combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native/combo_box.h>

#include <xview/panel.h>
#include <xview/openmenu.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    linux::openlook::openlook_combo_box *state(native::combo_box *owner) {
        return linux::openlook::combo_box_bindings.object_from_handle(owner);
    }

    void selected(Panel_item item, int value, Event *) {
        auto *owner = reinterpret_cast<native::combo_box *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto *binding = state(owner);
        if (owner && binding && !binding->suppress &&
            linux::openlook::permit_input(owner)) {
            owner->on_native_selection(value);
            if (binding->text) {
                binding->suppress = true;
                xv_set(binding->text,
                       PANEL_VALUE,
                       owner->get_text().c_str(),
                       nullptr);
                binding->suppress = false;
            }
            owner->on_native_drop_down(false);
        }
    }

    Xv_opaque menu_selected(Menu, Menu_item item) {
        auto *owner = reinterpret_cast<native::combo_box *>(
            xv_get(item, MENU_CLIENT_DATA));
        auto *binding = state(owner);
        if (!owner || !binding || binding->suppress ||
            !linux::openlook::permit_input(owner)) {
            return item;
        }
        const int value = static_cast<int>(
            xv_get(item, MENU_VALUE));
        owner->on_native_selection(value);
        if (binding->text) {
            binding->suppress = true;
            xv_set(binding->text,
                   PANEL_VALUE,
                   owner->get_text().c_str(),
                   nullptr);
            binding->suppress = false;
        }
        owner->on_native_drop_down(false);
        return item;
    }

    Panel_setting edited(Panel_item item, Event *) {
        auto *owner = reinterpret_cast<native::combo_box *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto *binding = state(owner);
        if (owner && binding && !binding->suppress &&
            linux::openlook::permit_input(owner)) {
            const char *value = reinterpret_cast<const char *>(
                xv_get(item, PANEL_VALUE));
            owner->on_native_text(value ? value : "");
        }
        return PANEL_NONE;
    }

    void add_choices(Panel_item choice,
                     const std::vector<std::string> &items) {
        for (std::size_t index = 0; index < items.size(); ++index)
            xv_set(choice, PANEL_CHOICE_STRING,
                   static_cast<int>(index), items[index].c_str(), nullptr);
    }

    Panel_item create_text(native::combo_box *owner, Panel panel) {
        const native::rect bounds = owner->get_bounds();
        const int choice_width = std::min<int>(
            bounds.d.w, bounds.d.h + 6);
        return static_cast<Panel_item>(xv_create(
            panel,
            PANEL_TEXT,
            PANEL_VALUE,
            owner->get_text().c_str(),
            PANEL_NOTIFY_LEVEL,
            PANEL_ALL,
            PANEL_NOTIFY_PROC,
            edited,
            PANEL_CLIENT_DATA,
            owner,
            PANEL_VALUE_DISPLAY_WIDTH,
            std::max(1, static_cast<int>(bounds.d.w) - choice_width),
            XV_X,
            bounds.p.x,
            XV_Y,
            bounds.p.y,
            XV_WIDTH,
            std::max(1,
                     static_cast<int>(bounds.d.w)-choice_width),
            XV_HEIGHT,
            bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
    }

    Menu create_menu(native::combo_box *owner) {
        Menu menu = static_cast<Menu>(xv_create(
            XV_NULL, MENU_COMMAND_MENU, nullptr));
        if (!menu)
            return XV_NULL;
        for (std::size_t index = 0;
             index < owner->get_items().size(); ++index) {
            Menu_item item = static_cast<Menu_item>(xv_create(
                XV_NULL,
                MENUITEM,
                MENU_STRING,
                owner->get_items()[index].c_str(),
                MENU_NOTIFY_PROC,
                menu_selected,
                MENU_CLIENT_DATA,
                owner,
                MENU_VALUE,
                static_cast<Xv_opaque>(index),
                nullptr));
            xv_set(menu, MENU_APPEND_ITEM, item, nullptr);
        }
        return menu;
    }

    Panel_item create_choice(native::combo_box *owner,
                             Panel panel,
                             Menu &menu) {
        const native::rect bounds = owner->get_bounds();
        const bool editable = owner->get_style() ==
            native::combo_box_style::editable;
        const int choice_width = editable
            ? std::min<int>(bounds.d.w, bounds.d.h + 6)
            : bounds.d.w;
        Panel_item choice = XV_NULL;
        if (editable) {
            menu = create_menu(owner);
            if (menu) {
                choice = static_cast<Panel_item>(xv_create(
                    panel,
                    PANEL_ABBREV_MENU_BUTTON,
                    PANEL_ITEM_MENU,
                    menu,
                    PANEL_CLIENT_DATA,
                    owner,
                    XV_X,
                    bounds.x2()-choice_width,
                    XV_Y,
                    bounds.p.y,
                    XV_WIDTH,
                    choice_width,
                    XV_HEIGHT,
                    bounds.d.h,
                    XV_SHOW,
                    FALSE,
                    nullptr));
            }
        } else {
            choice = static_cast<Panel_item>(xv_create(
                panel,
                PANEL_CHOICE_STACK,
                PANEL_NOTIFY_PROC,
                selected,
                PANEL_CLIENT_DATA,
                owner,
                XV_X,
                bounds.p.x,
                XV_Y,
                bounds.p.y,
                XV_WIDTH,
                choice_width,
                XV_HEIGHT,
                bounds.d.h,
                XV_SHOW,
                FALSE,
                nullptr));
        }
        if (!choice)
            return XV_NULL;
        if (!editable)
            add_choices(choice, owner->get_items());
        if (!editable && owner->get_selected_index() >= 0)
            xv_set(choice,
                   PANEL_VALUE,
                   owner->get_selected_index(),
                   nullptr);
        return choice;
    }
}

namespace native
{
    void combo_box::apply_items() {
        auto *binding = state(this);
        if (!binding || !binding->choice)
            throw std::runtime_error(
                "OpenLook/XView: missing combo box binding.");
        Panel panel = linux::openlook::parent_panel(this);
        Menu replacement_menu = XV_NULL;
        Panel_item replacement = create_choice(
            this, panel, replacement_menu);
        if (!replacement) {
            if (replacement_menu)
                xv_destroy_safe(replacement_menu);
            throw std::runtime_error(
                "OpenLook/XView: failed to update combo box items.");
        }
        const bool visible =
            static_cast<bool>(xv_get(binding->choice, XV_SHOW));
        Panel_item previous = binding->choice;
        Menu previous_menu = binding->menu;
        try {
            linux::openlook::wnd_bindings.register_pair(
                replacement, this);
        } catch (...) {
            xv_destroy_safe(replacement);
            throw;
        }
        binding->choice = replacement;
        binding->menu = replacement_menu;
        xv_destroy_safe(previous);
        if (previous_menu)
            xv_destroy_safe(previous_menu);
        if (visible)
            xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
    }

    void combo_box::apply_selected_index() {
        auto *binding = state(this);
        if (!binding || !binding->choice)
            throw std::runtime_error(
                "OpenLook/XView: missing combo box binding.");
        binding->suppress = true;
        if (binding->text && binding->menu &&
            get_selected_index() >= 0) {
            xv_set(binding->menu,
                   MENU_DEFAULT,
                   std::max(1, get_selected_index()+1),
                   nullptr);
        } else {
            xv_set(binding->choice, PANEL_VALUE,
                   std::max(0, get_selected_index()), nullptr);
        }
        binding->suppress = false;
    }

    void combo_box::apply_text() {
        auto *binding = state(this);
        if (!binding)
            throw std::runtime_error(
                "OpenLook/XView: missing combo box binding.");
        binding->suppress = true;
        if (binding->text)
            xv_set(binding->text, PANEL_VALUE, get_text().c_str(), nullptr);
        binding->suppress = false;
    }

    void combo_box::apply_style() {
        auto *binding = state(this);
        if (!binding || !binding->choice)
            throw std::runtime_error(
                "OpenLook/XView: missing combo box binding.");
        const bool visible =
            static_cast<bool>(xv_get(binding->choice, XV_SHOW));
        const bool editable =
            get_style() == combo_box_style::editable;
        if (editable && !binding->text) {
            binding->text = create_text(
                this, linux::openlook::parent_panel(this));
            if (!binding->text)
                throw std::runtime_error(
                    "OpenLook/XView: failed to edit combo box style.");
        } else if (!editable && binding->text) {
            xv_destroy_safe(binding->text);
            binding->text = XV_NULL;
        }
        apply_items();
        apply_bounds();
        if (visible) {
            if (binding->text)
                xv_set(binding->text, XV_SHOW, TRUE, nullptr);
            xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
        }
    }

    void combo_box::create_native() {
        auto *self = this;
        Panel panel = linux::openlook::parent_panel(self);
        auto *binding = new linux::openlook::openlook_combo_box;
        const bool editable =
            get_style() == combo_box_style::editable;
        if (editable)
            binding->text = create_text(self, panel);
        binding->choice = create_choice(
            self, panel, binding->menu);
        if (!binding->choice || (editable && !binding->text)) {
            if (binding->choice) xv_destroy_safe(binding->choice);
            if (binding->text) xv_destroy_safe(binding->text);
            if (binding->menu) xv_destroy_safe(binding->menu);
            delete binding;
            throw std::runtime_error(
                "OpenLook/XView: failed to create combo box.");
        }
        linux::openlook::wnd_bindings.register_pair(
            binding->choice, self);
        linux::openlook::combo_box_bindings.register_pair(self, binding);
    }

    void combo_box::show_native() {
        auto *binding = state(this);
        if (!_created || !binding)
            throw std::runtime_error(
                "OpenLook/XView: combo box is not created.");
        if (binding->text) xv_set(binding->text, XV_SHOW, TRUE, nullptr);
        xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *binding = state(self);
        linux::openlook::combo_box_bindings.unregister_by_handle(self);
        linux::openlook::wnd_bindings.unregister_by_object(self);
        if (binding) {
            if (binding->choice)
                xv_set(binding->choice, PANEL_CLIENT_DATA, nullptr, nullptr);
            if (binding->text)
                xv_set(binding->text, PANEL_CLIENT_DATA, nullptr, nullptr);
            if (binding->menu) {
                const int count = xv_get(binding->menu, MENU_NITEMS);
                for (int index = 1; index <= count; ++index) {
                    const auto item = xv_get(binding->menu, MENU_NTH_ITEM, index);
                    xv_set(item, MENU_CLIENT_DATA, nullptr, nullptr);
                }
            }
            if (binding->choice) xv_destroy_safe(binding->choice);
            if (binding->text) xv_destroy_safe(binding->text);
            if (binding->menu) xv_destroy_safe(binding->menu);
            delete binding;
        }
    }
} // namespace native
