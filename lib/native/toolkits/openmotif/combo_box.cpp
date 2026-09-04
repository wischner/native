//
// Implements the native Motif combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <X11/Intrinsic.h>
#include <Xm/ComboBox.h>
#include <Xm/List.h>
#include <Xm/TextF.h>
#include <Xm/Xm.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    Widget widget_for(native::combo_box *owner) {
        return linux::openmotif::combo_box_bindings
            .object_from_handle(owner);
    }

    void replace_items(Widget widget,
                       const std::vector<std::string> &items) {
        std::vector<XmString> values;
        values.reserve(items.size());
        for (const auto &item : items)
            values.push_back(XmStringCreateLocalized(
                const_cast<char *>(item.c_str())));
        XtVaSetValues(widget,
                      XmNitems, values.empty() ? nullptr : values.data(),
                      XmNitemCount, static_cast<int>(values.size()),
                      nullptr);
        for (XmString value : values)
            XmStringFree(value);
        XmComboBoxUpdate(widget);
    }

    void changed(Widget, XtPointer data, XtPointer call_data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *event = static_cast<XmComboBoxCallbackStruct *>(call_data);
        if (!owner || !event)
            return;
        char *text = nullptr;
        if (event->item_or_text && XmStringGetLtoR(
                event->item_or_text, XmFONTLIST_DEFAULT_TAG, &text) && text) {
            const std::string value(text);
            XtFree(text);
            const auto &items = owner->get_items();
            const auto found = std::find(items.begin(), items.end(), value);
            if (found != items.end()) {
                owner->on_native_selection(static_cast<int>(found-items.begin()));
                return;
            }
            owner->on_native_text(value);
        }
    }
}

namespace native
{
    void combo_box::apply_items() {
        Widget widget = widget_for(this);
        if (!widget)
            throw std::runtime_error("Motif: Missing combo box widget.");
        replace_items(widget, get_items());
    }

    void combo_box::apply_selected_index() {
        Widget widget = widget_for(this);
        if (!widget)
            throw std::runtime_error("Motif: Missing combo box widget.");
        const int selected = get_selected_index();
        if (selected >= 0) {
            XmString item = XmStringCreateLocalized(const_cast<char *>(
                get_items()[static_cast<std::size_t>(selected)].c_str()));
            XtVaSetValues(widget,
                          XmNselectedPosition, selected,
                          XmNselectedItem, item,
                          nullptr);
            Widget list = nullptr;
            XtVaGetValues(widget, XmNlist, &list, nullptr);
            if (list)
                XmListSelectPos(list, selected+1, False);
            XmStringFree(item);
        } else {
            Widget list = nullptr;
            XtVaGetValues(widget, XmNlist, &list, nullptr);
            if (list)
                XmListDeselectAllItems(list);
        }
    }

    void combo_box::apply_text() {
        Widget widget = widget_for(this);
        if (!widget)
            throw std::runtime_error("Motif: Missing combo box widget.");
        Widget text = nullptr;
        XtVaGetValues(widget, XmNtextField, &text, nullptr);
        if (text)
            XmTextFieldSetString(
                text, const_cast<char *>(get_text().c_str()));
    }

    void combo_box::apply_style() {
        Widget widget = widget_for(this);
        if (widget)
            XtVaSetValues(widget, XmNcomboBoxType,
                get_style() == combo_box_style::editable
                    ? XmDROP_DOWN_COMBO_BOX : XmDROP_DOWN_LIST,
                nullptr);
    }

    void combo_box::create_native() {
        auto *parent = get_parent();
        auto *self = this;
        Widget parent_widget = linux::openmotif::parent_widget(self);
        if (!parent || !parent->get_created() || !parent_widget)
            throw std::runtime_error(
                "Motif: combo box requires a created parent.");
        Arg arguments[16];
        Cardinal count = 0;
        XtSetArg(arguments[count], XmNx, _bounds.p.x); ++count;
        XtSetArg(arguments[count], XmNy, _bounds.p.y); ++count;
        XtSetArg(arguments[count], XmNwidth, _bounds.d.w); ++count;
        XtSetArg(arguments[count], XmNheight, _bounds.d.h); ++count;
        XtSetArg(arguments[count], XmNpositionMode, XmZERO_BASED); ++count;
        XtSetArg(arguments[count], XmNmarginHeight, 0); ++count;
        XtSetArg(arguments[count], XmNmarginWidth, 1); ++count;
        XtSetArg(arguments[count], XmNhighlightThickness, 0); ++count;
        XtSetArg(arguments[count], XmNarrowSpacing, 1); ++count;
        Widget widget = get_style() == combo_box_style::editable
            ? XmCreateDropDownComboBox(parent_widget,
                const_cast<char *>("comboBox"), arguments, count)
            : XmCreateDropDownList(parent_widget,
                const_cast<char *>("comboBox"), arguments, count);
        if (!widget)
            throw std::runtime_error(
                "Motif: Failed to create combo box.");
        linux::openmotif::wnd_bindings.register_pair(widget, self);
        linux::openmotif::combo_box_bindings.register_pair(self, widget);
        XtAddCallback(widget, XmNselectionCallback, changed, self);
        replace_items(widget, get_items());
        self->apply_selected_index();
        self->apply_text();
    }

    void combo_box::show_native() {
        Widget widget = widget_for(this);
        if (!_created || !widget)
            throw std::runtime_error("Motif: combo box is not created.");
        XtManageChild(widget);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        Widget widget = widget_for(self);
        linux::openmotif::combo_box_bindings.unregister_by_handle(self);
        if (widget) {
            linux::openmotif::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
