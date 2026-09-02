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
            owner->on_native_drop_down(false);
        }
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

    Panel_item create_choice(native::combo_box *owner, Panel panel) {
        const native::rect bounds = owner->get_bounds();
        const bool editable = owner->get_style() ==
            native::combo_box_style::editable;
        const int choice_width = editable
            ? std::min<int>(bounds.d.w, bounds.d.h + 6)
            : bounds.d.w;
        Panel_item choice = static_cast<Panel_item>(xv_create(
            panel,
            PANEL_CHOICE_STACK,
            PANEL_NOTIFY_PROC,
            selected,
            PANEL_CLIENT_DATA,
            owner,
            XV_X,
            editable ? bounds.x2()-choice_width : bounds.p.x,
            XV_Y,
            bounds.p.y,
            XV_WIDTH,
            choice_width,
            XV_HEIGHT,
            bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
        if (!choice)
            return XV_NULL;
        add_choices(choice, owner->get_items());
        if (owner->get_selected_index() >= 0)
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
        Panel_item replacement = create_choice(this, panel);
        if (!replacement)
            throw std::runtime_error(
                "OpenLook/XView: failed to update combo box items.");
        const bool visible =
            static_cast<bool>(xv_get(binding->choice, XV_SHOW));
        Panel_item previous = binding->choice;
        try {
            linux::openlook::wnd_bindings.register_pair(
                replacement, this);
        } catch (...) {
            xv_destroy_safe(replacement);
            throw;
        }
        binding->choice = replacement;
        xv_destroy_safe(previous);
        if (visible)
            xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
    }

    void combo_box::apply_selected_index() {
        auto *binding = state(this);
        if (!binding || !binding->choice)
            throw std::runtime_error(
                "OpenLook/XView: missing combo box binding.");
        binding->suppress = true;
        xv_set(binding->choice, PANEL_VALUE,
               std::max(0, get_selected_index()), nullptr);
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
        apply_bounds();
        if (visible) {
            if (binding->text)
                xv_set(binding->text, XV_SHOW, TRUE, nullptr);
            xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
        }
    }

    void combo_box::create() const {
        if (_created) return;
        auto *self = const_cast<combo_box *>(this);
        Panel panel = linux::openlook::parent_panel(self);
        auto *binding = new linux::openlook::openlook_combo_box;
        const bool editable =
            get_style() == combo_box_style::editable;
        if (editable)
            binding->text = create_text(self, panel);
        binding->choice = create_choice(self, panel);
        if (!binding->choice || (editable && !binding->text)) {
            if (binding->choice) xv_destroy_safe(binding->choice);
            if (binding->text) xv_destroy_safe(binding->text);
            delete binding;
            throw std::runtime_error(
                "OpenLook/XView: failed to create combo box.");
        }
        linux::openlook::wnd_bindings.register_pair(
            binding->choice, self);
        linux::openlook::combo_box_bindings.register_pair(self, binding);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *binding = state(const_cast<combo_box *>(this));
        if (!_created || !binding)
            throw std::runtime_error(
                "OpenLook/XView: combo box is not created.");
        if (binding->text) xv_set(binding->text, XV_SHOW, TRUE, nullptr);
        xv_set(binding->choice, XV_SHOW, TRUE, nullptr);
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *binding = state(self);
        self->on_native_destroy();
        linux::openlook::combo_box_bindings.unregister_by_handle(self);
        linux::openlook::wnd_bindings.unregister_by_object(self);
        if (binding) {
            if (binding->choice) xv_destroy_safe(binding->choice);
            if (binding->text) xv_destroy_safe(binding->text);
            delete binding;
        }
    }
} // namespace native
