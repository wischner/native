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
    constexpr unsigned int bitmap_arrow_width = 9;
    constexpr unsigned int bitmap_arrow_height = 5;
    constexpr char bitmap_arrow_bits[] = {
        static_cast<char>(0xff), 0x01,
        static_cast<char>(0xfe), 0x00,
        0x7c, 0x00,
        0x38, 0x00,
        0x10, 0x00
    };

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
            XtNborderWidth,
            1,
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

    void combo_box::create_native() {
        Widget parent = get_parent()
            ? linux::x11::wnd_bindings.handle_from_object(get_parent())
            : nullptr;
        if (!parent || !get_parent()->get_created())
            throw std::runtime_error(
                "X11: combo box requires a created parent.");
        auto *self = this;
        auto *binding = new linux::x11::xaw_combo_box;
        binding->root = XtVaCreateWidget(
            "comboBox", formWidgetClass, parent,
            XtNhorizDistance, _bounds.p.x,
            XtNvertDistance, _bounds.p.y,
            XtNwidth, linux::x11::widget_dimension(_bounds.d.w),
            XtNheight, linux::x11::widget_dimension(_bounds.d.h),
            XtNborderWidth, 0,
            XtNdefaultDistance, 0,
            XtNleft, XtChainLeft,
            XtNright, XtChainLeft,
            XtNtop, XtChainTop,
            XtNbottom, XtChainTop,
            XtNresizable, False,
            nullptr);
        const int button_width = std::min<int>(_bounds.d.w, _bounds.d.h+4);
        const int child_height = std::max(1,
            static_cast<int>(_bounds.d.h)-2);
        const int text_width = std::max(1,
            static_cast<int>(_bounds.d.w)-button_width-2);
        const int arrow_width = std::max(1, button_width-2);
        binding->text = XtVaCreateManagedWidget(
            "comboText", asciiTextWidgetClass, binding->root,
            XtNhorizDistance, 0,
            XtNvertDistance, 0,
            XtNwidth, text_width,
            XtNheight, child_height,
            XtNborderWidth, 1,
            XtNstring, get_text().c_str(),
            XtNeditType, get_style() == combo_box_style::editable
                ? XawtextEdit : XawtextRead,
            XtNleft, XtChainLeft, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        binding->button = XtVaCreateManagedWidget(
            "comboButton", menuButtonWidgetClass, binding->root,
            XtNfromHoriz, binding->text,
            XtNhorizDistance, 0,
            XtNvertDistance, 0,
            XtNwidth, arrow_width,
            XtNheight, child_height,
            XtNborderWidth, 1,
            XtNlabel, "", XtNmenuName, "comboMenu",
            XtNleft, XtChainRight, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        if (linux::x11::cached_display) {
            binding->arrow = XCreateBitmapFromData(
                linux::x11::cached_display,
                RootWindowOfScreen(XtScreen(binding->root)),
                bitmap_arrow_bits,
                bitmap_arrow_width,
                bitmap_arrow_height);
            if (binding->arrow != None)
                XtVaSetValues(binding->button,
                              XtNbitmap,
                              binding->arrow,
                              nullptr);
        }
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
    }

    void combo_box::show_native() {
        auto *binding = state(this);
        if (!_created || !binding)
            throw std::runtime_error("X11: combo box is not created.");
        XtManageChild(binding->root);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *binding = state(self);
        linux::x11::combo_box_bindings.unregister_by_handle(self);
        linux::x11::wnd_bindings.unregister_by_object(self);
        if (binding) {
            if (binding->root) XtDestroyWidget(binding->root);
            if (binding->arrow != None &&
                linux::x11::cached_display) {
                XSync(linux::x11::cached_display, False);
                XFreePixmap(linux::x11::cached_display,
                            binding->arrow);
            }
            clear_callbacks(binding); delete binding;
        }
    }
} // namespace native
