//
// Implements an Athena text/menu combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    linux::x11::xaw_combo_box *state(native::combo_box *owner) {
        return linux::x11::combo_box_bindings.object_from_handle(owner);
    }

    void selected(Widget, XtPointer data, XtPointer) {
        auto *callback = static_cast<linux::x11::xaw_combo_callback *>(data);
        if (!callback || !callback->owner) return;
        callback->owner->on_native_selection(callback->index);
        callback->owner->on_native_drop_down(false);
    }

    void text_event(Widget widget, XtPointer data,
                    XEvent *event, Boolean *) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *binding = state(owner);
        if (!owner || !binding || binding->suppress ||
            !event || event->type != KeyRelease) return;
        char *value = nullptr;
        XtVaGetValues(widget, XtNstring, &value, nullptr);
        owner->on_native_text(value ? value : "");
    }

    void clear_callbacks(linux::x11::xaw_combo_box *binding) {
        for (auto *callback : binding->callbacks) delete callback;
        binding->callbacks.clear();
    }

    void create_menu(native::combo_box *owner,
                     linux::x11::xaw_combo_box *binding) {
        binding->menu = XtVaCreatePopupShell(
            "comboMenu",
            simpleMenuWidgetClass,
            binding->root,
            nullptr);
        for (std::size_t index = 0;
             index < owner->get_items().size();
             ++index) {
            Widget entry = XtVaCreateManagedWidget(
                "item",
                smeBSBObjectClass,
                binding->menu,
                XtNlabel,
                owner->get_items()[index].c_str(),
                nullptr);
            auto *callback = new linux::x11::xaw_combo_callback;
            callback->owner = owner;
            callback->index = static_cast<int>(index);
            binding->callbacks.push_back(callback);
            XtAddCallback(entry, XtNcallback, selected, callback);
        }
    }
}

namespace native
{
    void combo_box::apply_items() {
        auto *binding = state(this);
        if (!binding || !binding->root)
            throw std::runtime_error(
                "X11: Missing combo box binding.");
        clear_callbacks(binding);
        if (binding->menu)
            XtDestroyWidget(binding->menu);
        binding->menu = nullptr;
        create_menu(this, binding);
    }

    void combo_box::apply_selected_index() { apply_text(); }

    void combo_box::apply_text() {
        auto *binding = state(this);
        if (!binding || !binding->text)
            throw std::runtime_error("X11: Missing combo box binding.");
        binding->suppress = true;
        XtVaSetValues(binding->text, XtNstring, get_text().c_str(), nullptr);
        binding->suppress = false;
    }

    void combo_box::apply_style() {
        auto *binding = state(this);
        if (binding && binding->text)
            XtVaSetValues(binding->text, XtNeditType,
                get_style() == combo_box_style::editable
                    ? XawtextEdit : XawtextRead, nullptr);
    }

    void combo_box::create() const {
        if (_created) return;
        Widget parent = get_parent()
            ? linux::x11::wnd_bindings.handle_from_object(get_parent())
            : nullptr;
        if (!parent || !get_parent()->get_created())
            throw std::runtime_error(
                "X11: combo box requires a created parent.");
        auto *self = const_cast<combo_box *>(this);
        auto *binding = new linux::x11::xaw_combo_box;
        binding->root = XtVaCreateWidget(
            "comboBox", formWidgetClass, parent,
            XtNx, _bounds.p.x, XtNy, _bounds.p.y,
            XtNwidth, _bounds.d.w, XtNheight, _bounds.d.h,
            XtNborderWidth, 0, nullptr);
        const int button_width = std::min<int>(_bounds.d.w, _bounds.d.h+4);
        binding->text = XtVaCreateManagedWidget(
            "comboText", asciiTextWidgetClass, binding->root,
            XtNx, 0, XtNy, 0,
            XtNwidth, std::max(1, static_cast<int>(_bounds.d.w)-button_width),
            XtNheight, _bounds.d.h,
            XtNstring, get_text().c_str(),
            XtNeditType, get_style() == combo_box_style::editable
                ? XawtextEdit : XawtextRead,
            XtNleft, XtChainLeft, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        binding->button = XtVaCreateManagedWidget(
            "comboButton", menuButtonWidgetClass, binding->root,
            XtNx, static_cast<int>(_bounds.d.w)-button_width, XtNy, 0,
            XtNwidth, button_width, XtNheight, _bounds.d.h,
            XtNlabel, "v", XtNmenuName, "comboMenu",
            XtNleft, XtChainRight, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        create_menu(self, binding);
        if (!binding->root || !binding->text || !binding->button ||
            !binding->menu) {
            if (binding->root) XtDestroyWidget(binding->root);
            clear_callbacks(binding); delete binding;
            throw std::runtime_error("X11: Failed to create combo box.");
        }
        XtAddEventHandler(binding->text, KeyReleaseMask, False,
                          text_event, self);
        linux::x11::wnd_bindings.register_pair(binding->root, self);
        linux::x11::combo_box_bindings.register_pair(self, binding);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *binding = state(const_cast<combo_box *>(this));
        if (!_created || !binding)
            throw std::runtime_error("X11: combo box is not created.");
        XtManageChild(binding->root);
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *binding = state(self);
        self->on_native_destroy();
        linux::x11::combo_box_bindings.unregister_by_handle(self);
        linux::x11::wnd_bindings.unregister_by_object(self);
        if (binding) {
            if (binding->root) XtDestroyWidget(binding->root);
            clear_callbacks(binding); delete binding;
        }
    }
} // namespace native
