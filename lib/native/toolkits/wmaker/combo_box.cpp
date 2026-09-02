//
// Implements a native WINGs combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    linux::wmaker::native_combo_box *binding(native::combo_box *owner) {
        return linux::wmaker::combo_box_bindings.object_from_handle(owner);
    }

    void popup_changed(WMWidget *, void *data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *state = binding(owner);
        if (!owner || !state || state->suppress) return;
        const int index = WMGetPopUpButtonSelectedItem(state->popup);
        linux::wmaker::defer([owner, index]() {
            if (owner->get_created()) {
                owner->on_native_selection(index);
                owner->on_native_drop_down(false);
            }
        });
    }

    void field_changed(WMTextFieldDelegate *delegate, WMNotification *) {
        auto *owner = delegate
            ? static_cast<native::combo_box *>(delegate->data) : nullptr;
        auto *state = binding(owner);
        if (!owner || !state || state->suppress) return;
        char *value = WMGetTextFieldText(state->field);
        const std::string text = value ? value : "";
        if (value) std::free(value);
        linux::wmaker::defer([owner, text]() {
            if (owner->get_created()) owner->on_native_text(text);
        });
    }

    void replace(linux::wmaker::native_combo_box *state,
                 const std::vector<std::string> &items) {
        while (WMGetPopUpButtonNumberOfItems(state->popup) > 0)
            WMRemovePopUpButtonItem(state->popup, 0);
        for (const auto &item : items)
            WMAddPopUpButtonItem(state->popup, item.c_str());
    }
}

namespace linux::wmaker
{
    void configure_combo_box(native::combo_box &owner,
                             native_combo_box &state) {
        const native::size dimensions = owner.get_dimensions();
        const bool editable = owner.get_style() ==
            native::combo_box_style::editable;
        const int popup_width = editable
            ? std::min<int>(dimensions.w, dimensions.h + 4)
            : dimensions.w;
        WMResizeWidget(
            state.field,
            std::max(1,
                     static_cast<int>(dimensions.w)-popup_width),
            dimensions.h);
        WMMoveWidget(
            state.popup,
            editable
                ? static_cast<int>(dimensions.w)-popup_width
                : 0,
            0);
        WMResizeWidget(state.popup, popup_width, dimensions.h);
        WMSetPopUpButtonPullsDown(
            state.popup, editable ? True : False);
    }
} // namespace linux::wmaker

namespace native
{
    void combo_box::apply_items() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        replace(state, get_items());
        state->suppress = false;
    }

    void combo_box::apply_selected_index() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        WMSetPopUpButtonSelectedItem(state->popup, get_selected_index());
        state->suppress = false;
    }

    void combo_box::apply_text() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        WMSetPopUpButtonText(state->popup, get_text().c_str());
        if (state->field)
            WMSetTextFieldText(state->field, get_text().c_str());
        state->suppress = false;
    }

    void combo_box::apply_style() {
        auto *state = binding(this);
        if (!state || !state->frame || !state->field || !state->popup)
            throw std::runtime_error(
                "Window Maker/WINGs: missing combo box binding.");
        const bool editable =
            get_style() == combo_box_style::editable;
        linux::wmaker::configure_combo_box(*this, *state);

        if (WMWidgetXID(state->frame) != None) {
            if (editable) {
                WMRealizeWidget(state->field);
                WMMapWidget(state->field);
            } else {
                WMUnmapWidget(state->field);
            }
        }
    }

    void combo_box::create() const {
        if (_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = new linux::wmaker::native_combo_box;
        state->frame = WMCreateFrame(linux::wmaker::parent_widget(self));
        state->popup = WMCreatePopUpButton(state->frame);
        if (!state->frame || !state->popup) {
            if (state->frame) WMDestroyWidget(state->frame);
            delete state;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create combo box.");
        }
        WMSetFrameRelief(state->frame, WRFlat);
        const point position = linux::wmaker::control_position(self);
        WMMoveWidget(state->frame, position.x, position.y);
        WMResizeWidget(state->frame, _bounds.d.w, _bounds.d.h);
        state->field = WMCreateTextField(state->frame);
        if (!state->field) {
            WMDestroyWidget(state->frame);
            delete state;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create combo box.");
        }
        state->delegate.data = self;
        state->delegate.didChange = field_changed;
        WMSetTextFieldDelegate(state->field, &state->delegate);
        WMSetTextFieldText(state->field, get_text().c_str());
        linux::wmaker::configure_combo_box(*self, *state);
        WMSetPopUpButtonAction(state->popup, popup_changed, self);
        replace(state, get_items());
        if (get_selected_index() >= 0)
            WMSetPopUpButtonSelectedItem(state->popup, get_selected_index());
        linux::wmaker::wnd_bindings.register_pair(state->frame, self);
        linux::wmaker::combo_box_bindings.register_pair(self, state);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *state = binding(const_cast<combo_box *>(this));
        if (!_created || !state)
            throw std::runtime_error(
                "Window Maker/WINGs: combo box is not created.");
        WMRealizeWidget(state->frame);
        WMMapSubwidgets(state->frame);
        if (get_style() != combo_box_style::editable)
            WMUnmapWidget(state->field);
        WMMapWidget(state->frame);
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = binding(self);
        self->on_native_destroy();
        linux::wmaker::combo_box_bindings.unregister_by_handle(self);
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (state && state->frame) WMDestroyWidget(state->frame);
        delete state;
    }
} // namespace native
