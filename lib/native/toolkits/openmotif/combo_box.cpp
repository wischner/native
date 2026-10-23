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
#include "control_font.h"
#include "../../combo_picker.h"
#include <Xm/ArrowB.h>
#include <Xm/Form.h>
#include <X11/keysym.h>

namespace
{
    struct combo_state
    {
        Widget text = nullptr;
        Widget arrows[2]{};
        bool composed = false;
        bool initialized = false;
        bool suppress = false;
    };

    // Read selection before insertion position so a highlighted
    // component remains the target even when its caret sits at the next
    // separator.
    std::size_t caret_position(Widget text) {
        XmTextPosition first = 0, last = 0;
        if (XmTextFieldGetSelectionPosition(text, &first, &last))
            return std::size_t(first);
        return std::size_t(XmTextFieldGetInsertionPosition(text));
    }

    void arrow_clicked(Widget arrow, XtPointer data, XtPointer) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *actions = native::detail::peer_state<
            native::detail::combo_picker_actions>(*owner);
        if (!actions)
            return;
        const auto open = actions->open;
        const auto step = actions->step;
        const auto commit = actions->commit;
        const auto *state =
            native::detail::peer_state<combo_state>(*owner);
        const int direction = arrow == state->arrows[0] ? 1 : -1;
        if (open)
            open();
        else if (step) {
            const native::detail::window_guard guard(*owner);
            if (commit)
                commit();
            if (guard.alive())
                step(direction);
        }
    }

    void text_changed(Widget text, XtPointer data, XtPointer) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *state = native::detail::peer_state<combo_state>(*owner);
        if (!state || state->suppress)
            return;
        char *value = XmTextFieldGetString(text);
        const std::string contents = value ? value : "";
        XtFree(value);
        const native::detail::window_guard guard(*owner);
        owner->on_native_text(contents);
        if (!guard.alive())
            return;
        if (state->composed)
            return;
        Widget list = nullptr;
        XtVaGetValues(
            linux::openmotif::wnd_bindings.handle_from_object(owner),
            XmNlist,
            &list,
            nullptr);
        if (list) {
            if (owner->get_selected_index() >= 0)
                XmListSelectPos(
                    list, owner->get_selected_index() + 1, False);
            else
                XmListDeselectAllItems(list);
        }
    }
    void
    text_key(Widget, XtPointer data, XEvent *event, Boolean *dispatch) {
        auto *owner = static_cast<native::combo_box *>(data);
        const auto *actions = native::detail::peer_state<
            native::detail::combo_picker_actions>(*owner);
        const bool readonly_click = event->type == ButtonRelease &&
                                    actions && actions->read_only;
        if (!readonly_click &&
            (event->type != KeyPress ||
             XLookupKeysym(&event->xkey, 0) != XK_Return))
            return;
        if (auto *actions = native::detail::peer_state<
                native::detail::combo_picker_actions>(*owner)) {
            const auto commit =
                actions->read_only ? actions->open : actions->commit;
            *dispatch = False;
            if (commit)
                commit();
        }
    }

    // Own all picker children inside one native parent. There is no
    // hidden XmComboBox list shell and no sibling overlay to be
    // covered.
    Widget create_picker(native::combo_box &owner,
                         Widget parent,
                         combo_state &state) {
        auto &actions = *native::detail::peer_state<
            native::detail::combo_picker_actions>(owner);
        const auto bounds = owner.get_bounds();
        Widget frame = XtVaCreateWidget("comboBox",
                                        xmFormWidgetClass,
                                        parent,
                                        XmNx,
                                        bounds.x1(),
                                        XmNy,
                                        bounds.y1(),
                                        XmNwidth,
                                        bounds.w(),
                                        XmNheight,
                                        bounds.h(),
                                        XmNresizePolicy,
                                        XmRESIZE_NONE,
                                        XmNshadowThickness,
                                        2,
                                        XmNshadowType,
                                        XmSHADOW_IN,
                                        nullptr);
        state.composed = true;
        const int count = actions.step ? 2 : 1;
        for (int index = 0; index < count; ++index) {
            state.arrows[index] = XtVaCreateManagedWidget(
                index == 0 ? "pickerArrowUp" : "pickerArrowDown",
                xmArrowButtonWidgetClass,
                frame,
                XmNwidth,
                std::max(16, int(bounds.h()) - 4),
                XmNhighlightThickness,
                actions.step ? 0 : 2,
                XmNshadowThickness,
                1,
                XmNtraversalOn,
                False,
                XmNarrowDirection,
                actions.step && index == 0 ? XmARROW_UP : XmARROW_DOWN,
                XmNrightAttachment,
                XmATTACH_FORM,
                XmNrightOffset,
                2,
                XmNtopAttachment,
                index == 0 ? XmATTACH_FORM : XmATTACH_POSITION,
                XmNtopPosition,
                50,
                XmNtopOffset,
                index == 0 ? 2 : 0,
                XmNbottomAttachment,
                count == 2 && index == 0 ? XmATTACH_POSITION
                                         : XmATTACH_FORM,
                XmNbottomPosition,
                50,
                XmNbottomOffset,
                count == 2 && index == 0 ? 0 : 2,
                nullptr);
            XtAddCallback(state.arrows[index],
                          XmNactivateCallback,
                          arrow_clicked,
                          &owner);
        }
        state.text = XtVaCreateManagedWidget("comboText",
                                             xmTextFieldWidgetClass,
                                             frame,
                                             XmNshadowThickness,
                                             0,
                                             XmNhighlightThickness,
                                             0,
                                             XmNmarginWidth,
                                             4,
                                             XmNmarginHeight,
                                             2,
                                             XmNeditable,
                                             !actions.read_only,
                                             XmNleftAttachment,
                                             XmATTACH_FORM,
                                             XmNleftOffset,
                                             2,
                                             XmNtopAttachment,
                                             XmATTACH_FORM,
                                             XmNtopOffset,
                                             2,
                                             XmNbottomAttachment,
                                             XmATTACH_FORM,
                                             XmNbottomOffset,
                                             2,
                                             XmNrightAttachment,
                                             XmATTACH_WIDGET,
                                             XmNrightWidget,
                                             state.arrows[0],
                                             nullptr);
        actions.caret = [text = state.text] {
            return caret_position(text);
        };
        return frame;
    }

    Widget widget_for(native::combo_box *owner) {
        return linux::openmotif::wnd_bindings.handle_from_object(owner);
    }

    void replace_items(Widget widget,
                       const std::vector<std::string> &items) {
        std::vector<XmString> values;
        values.reserve(items.size());
        for (const auto &item : items)
            values.push_back(XmStringCreateLocalized(
                const_cast<char *>(item.c_str())));
        XtVaSetValues(widget,
                      XmNitems,
                      values.empty() ? nullptr : values.data(),
                      XmNitemCount,
                      static_cast<int>(values.size()),
                      nullptr);
        for (XmString value : values)
            XmStringFree(value);
        XmComboBoxUpdate(widget);
    }

    void changed(Widget, XtPointer data, XtPointer call_data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *event =
            static_cast<XmComboBoxCallbackStruct *>(call_data);
        if (!owner || !event)
            return;
        char *text = nullptr;
        if (event->item_or_text &&
            XmStringGetLtoR(
                event->item_or_text, XmFONTLIST_DEFAULT_TAG, &text) &&
            text) {
            const std::string value(text);
            XtFree(text);
            const auto &items = owner->get_items();
            const auto found =
                std::find(items.begin(), items.end(), value);
            if (found != items.end()) {
                owner->on_native_selection(
                    static_cast<int>(found - items.begin()));
                return;
            }
            owner->on_native_text(value);
        }
    }
} // namespace

namespace native
{
    void combo_box::apply_items() {
        Widget widget = widget_for(this);
        if (!widget)
            throw std::runtime_error(
                "Motif: Missing combo box widget.");
        const auto *state = detail::peer_state<combo_state>(*this);
        if (!state->composed)
            replace_items(widget, get_items());
    }

    void combo_box::apply_selected_index() {
        Widget widget = widget_for(this);
        if (!widget)
            throw std::runtime_error(
                "Motif: Missing combo box widget.");
        const auto *state = detail::peer_state<combo_state>(*this);
        if (state->composed)
            return;
        const int selected = get_selected_index();
        if (selected >= 0) {
            XmString item = XmStringCreateLocalized(const_cast<char *>(
                get_items()[static_cast<std::size_t>(selected)]
                    .c_str()));
            XtVaSetValues(widget,
                          XmNselectedPosition,
                          selected,
                          XmNselectedItem,
                          item,
                          nullptr);
            Widget list = nullptr;
            XtVaGetValues(widget, XmNlist, &list, nullptr);
            if (list)
                XmListSelectPos(list, selected + 1, False);
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
            throw std::runtime_error(
                "Motif: Missing combo box widget.");
        auto *state = detail::peer_state<combo_state>(*this);
        Widget text = state->text;
        if (!text)
            return;
        char *value = XmTextFieldGetString(text);
        const bool changed = get_text() != (value ? value : "");
        XtFree(value);
        state->suppress = true;
        if (changed) {
            const auto *actions =
                detail::peer_state<detail::combo_picker_actions>(*this);
            const auto caret =
                !state->initialized && actions && actions->step
                    ? 3
                    : XmTextFieldGetInsertionPosition(text);
            XmTextPosition first = 0, last = 0;
            const bool selected =
                state->initialized &&
                XmTextFieldGetSelectionPosition(text, &first, &last);
            XmTextFieldSetString(
                text, const_cast<char *>(get_text().c_str()));
            XmTextFieldSetInsertionPosition(
                text,
                std::min<XmTextPosition>(caret, get_text().size()));
            if (selected)
                XmTextFieldSetSelection(
                    text,
                    first,
                    last,
                    XtLastTimestampProcessed(XtDisplay(text)));
            state->initialized = true;
        }
        state->suppress = false;
    }

    void combo_box::apply_style() {
        Widget widget = widget_for(this);
        const auto *state = detail::peer_state<combo_state>(*this);
        if (widget && !state->composed)
            XtVaSetValues(widget,
                          XmNcomboBoxType,
                          get_style() == combo_box_style::editable
                              ? XmDROP_DOWN_COMBO_BOX
                              : XmDROP_DOWN_LIST,
                          nullptr);
    }

    void combo_box::create_native() {
        auto *parent = get_parent();
        auto *self = this;
        Widget parent_widget = linux::openmotif::parent_widget(self);
        if (!parent || !parent->get_created() || !parent_widget)
            throw std::runtime_error(
                "Motif: combo box requires a created parent.");
        auto *state = new combo_state;
        detail::assign_peer_state(*this, state);
        Widget widget = nullptr;
        if (detail::peer_state<detail::combo_picker_actions>(*this))
            widget = create_picker(*this, parent_widget, *state);
        else {
            Arg arguments[16];
            Cardinal count = 0;
            XtSetArg(arguments[count], XmNx, _bounds.p.x);
            ++count;
            XtSetArg(arguments[count], XmNy, _bounds.p.y);
            ++count;
            XtSetArg(arguments[count], XmNwidth, _bounds.d.w);
            ++count;
            XtSetArg(arguments[count], XmNheight, _bounds.d.h);
            ++count;
            XtSetArg(arguments[count], XmNpositionMode, XmZERO_BASED);
            ++count;
            XtSetArg(arguments[count], XmNmarginHeight, 0);
            ++count;
            XtSetArg(arguments[count], XmNmarginWidth, 1);
            ++count;
            XtSetArg(arguments[count], XmNhighlightThickness, 0);
            ++count;
            XtSetArg(arguments[count], XmNarrowSpacing, 1);
            ++count;
            widget = get_style() == combo_box_style::editable
                         ? XmCreateDropDownComboBox(
                               parent_widget,
                               const_cast<char *>("comboBox"),
                               arguments,
                               count)
                         : XmCreateDropDownList(
                               parent_widget,
                               const_cast<char *>("comboBox"),
                               arguments,
                               count);
        }
        if (!widget)
            throw std::runtime_error(
                "Motif: Failed to create combo box.");
        linux::openmotif::wnd_bindings.register_pair(widget, self);
        if (!state->composed)
            XtVaGetValues(widget, XmNtextField, &state->text, nullptr);
        Widget text = state->text;
        if (text) {
            linux::openmotif::apply_control_font(text);
            XtVaSetValues(
                text, XmNmarginHeight, 2, XmNmarginWidth, 4, nullptr);
            XtAddCallback(
                text, XmNvalueChangedCallback, text_changed, self);
            XtAddEventHandler(text,
                              KeyPressMask | ButtonReleaseMask,
                              False,
                              text_key,
                              self);
        }
        if (!state->composed) {
            XtAddCallback(widget, XmNselectionCallback, changed, self);
            replace_items(widget, get_items());
        }
        self->apply_selected_index();
        self->apply_text();
    }

    void combo_box::show_native() {
        Widget widget = widget_for(this);
        if (!_created || !widget)
            throw std::runtime_error(
                "Motif: combo box is not created.");
        XtManageChild(widget);
    }

    void combo_box::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        Widget widget = widget_for(self);
        // Remove the composite from traversal while its text/list
        // children are still alive. Focus callbacks during child
        // destruction must not try to traverse back into a
        // half-destroyed combo box.
        if (widget) {
            XtVaSetValues(widget, XmNtraversalOn, False, nullptr);
            XtUnmanageChild(widget);
        }
        if (widget) {
            linux::openmotif::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native

namespace native::detail
{
    void configure_picker_day(button &day, bool enabled) {
        auto widget =
            linux::openmotif::wnd_bindings.handle_from_object(&day);
        XtVaSetValues(widget,
                      XmNrecomputeSize,
                      False,
                      XmNwidth,
                      day.get_dimensions().w,
                      XmNheight,
                      day.get_dimensions().h,
                      nullptr);
        XtSetSensitive(widget, enabled);
        XtSetMappedWhenManaged(widget, !day.get_text().empty());
    }
    point picker_popup_position(combo_box &field) {
        auto widget =
            linux::openmotif::wnd_bindings.handle_from_object(&field);
        Dimension height = 0;
        XtVaGetValues(widget, XmNheight, &height, nullptr);
        Position x = 0, y = 0;
        XtTranslateCoords(widget, 0, height, &x, &y);
        return point(x, y);
    }
} // namespace native::detail
