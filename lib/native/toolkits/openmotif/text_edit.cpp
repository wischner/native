//
// Implements native Motif single-line and multiline text editors with
// live complete-value validation and portable clipboard commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    Widget parent_widget(native::text_edit *editor) {
        native::wnd *parent = editor->get_parent();
        Widget widget = parent
                            ? linux::openmotif::wnd_bindings
                                  .handle_from_object(parent)
                            : nullptr;
        if (!parent || !parent->get_created() || !widget) {
            throw std::runtime_error(
                "Motif: text_edit requires a created parent.");
        }
        return widget;
    }

    std::string widget_text(Widget widget, bool multiline) {
        char *value = multiline ? XmTextGetString(widget)
                                : XmTextFieldGetString(widget);
        const std::string result = value ? value : "";
        if (value)
            XtFree(value);
        return result;
    }

    std::size_t byte_offset(const std::string &text,
                            XmTextPosition characters) {
        std::size_t offset = 0;
        while (characters > 0 && offset < text.size()) {
            offset = native::detail::next_utf8(text, offset);
            --characters;
        }
        return offset;
    }

    XmTextPosition character_count(const std::string &text) {
        XmTextPosition count = 0;
        std::size_t offset = 0;
        while (offset < text.size()) {
            offset = native::detail::next_utf8(text, offset);
            ++count;
        }
        return count;
    }

    void verify_change(Widget widget,
                       XtPointer client_data,
                       XtPointer call_data) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        auto *binding = owner
                            ? linux::openmotif::text_edit_bindings
                                  .object_from_handle(owner)
                            : nullptr;
        auto *change = static_cast<XmTextVerifyCallbackStruct *>(
            call_data);
        if (!owner || !binding || binding->suppress || !change)
            return;
        std::string candidate = widget_text(widget, binding->multiline);
        const char *inserted = change->text && change->text->ptr
                                   ? change->text->ptr
                                   : "";
        const std::size_t count = change->text
                                      ? static_cast<std::size_t>(
                                            change->text->length)
                                      : 0;
        const std::size_t start =
            byte_offset(candidate, change->startPos);
        const std::size_t finish =
            byte_offset(candidate, change->endPos);
        candidate.replace(start,
                          finish - start,
                          inserted,
                          count);
        if (!owner->validate(candidate))
            change->doit = False;
    }

    void value_changed(Widget widget,
                       XtPointer client_data,
                       XtPointer) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        auto *binding = owner
                            ? linux::openmotif::text_edit_bindings
                                  .object_from_handle(owner)
                            : nullptr;
        if (!owner || !binding || binding->suppress)
            return;
        owner->on_native_text(widget_text(widget, binding->multiline));
    }

    void key_pressed(Widget,
                     XtPointer client_data,
                     XEvent *event,
                     Boolean *continue_dispatch) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        if (!owner || !event || event->type != KeyPress ||
            (event->xkey.state & ControlMask) == 0)
            return;
        char buffer[8] = {};
        KeySym symbol = NoSymbol;
        XLookupString(&event->xkey,
                      buffer,
                      sizeof(buffer),
                      &symbol,
                      nullptr);
        bool handled = true;
        switch (symbol) {
        case XK_a:
        case XK_A:
            owner->select_all();
            break;
        case XK_c:
        case XK_C:
            owner->copy();
            break;
        case XK_x:
        case XK_X:
            owner->cut();
            break;
        case XK_v:
        case XK_V:
            owner->paste();
            break;
        default:
            handled = false;
            break;
        }
        if (handled && continue_dispatch)
            *continue_dispatch = False;
    }
} // namespace

namespace native
{
    void text_edit::apply_text() {
        auto *binding = linux::openmotif::text_edit_bindings
                            .object_from_handle(this);
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "Motif: Missing text-edit widget.");
        binding->suppress = true;
        if (binding->multiline) {
            XmTextSetString(binding->widget,
                            const_cast<char *>(_text.c_str()));
        } else {
            XmTextFieldSetString(binding->widget,
                                 const_cast<char *>(_text.c_str()));
        }
        binding->suppress = false;
    }

    void text_edit::apply_read_only() {
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "Motif: Missing text-edit widget.");
        XtVaSetValues(widget, XmNeditable, !_read_only, nullptr);
    }

    void text_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        const bool multiline =
            _mode == text_edit_mode::multi_line;
        Widget widget = XtVaCreateWidget(
            "textEdit",
            multiline ? xmTextWidgetClass : xmTextFieldWidgetClass,
            parent_widget(self),
            XmNx,
            _bounds.p.x,
            XmNy,
            _bounds.p.y,
            XmNwidth,
            _bounds.d.w,
            XmNheight,
            _bounds.d.h,
            XmNvalue,
            _text.c_str(),
            XmNeditable,
            !_read_only,
            XmNeditMode,
            multiline ? XmMULTI_LINE_EDIT : XmSINGLE_LINE_EDIT,
            XmNwordWrap,
            multiline,
            nullptr);
        if (!widget)
            throw std::runtime_error(
                "Motif: Failed to create text_edit.");
        auto *binding = new linux::openmotif::motif_text_edit;
        binding->widget = widget;
        binding->multiline = multiline;
        linux::openmotif::wnd_bindings.register_pair(widget, self);
        linux::openmotif::text_edit_bindings.register_pair(
            self, binding);
        XtAddCallback(widget,
                      XmNmodifyVerifyCallback,
                      verify_change,
                      self);
        XtAddCallback(widget,
                      XmNvalueChangedCallback,
                      value_changed,
                      self);
        XtAddEventHandler(widget,
                          KeyPressMask,
                          False,
                          key_pressed,
                          self);
        _created = true;
        self->on_native_create();
    }

    void text_edit::show() const {
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(
                const_cast<text_edit *>(this));
        if (!_created || !widget)
            throw std::runtime_error(
                "Motif: text_edit is not created.");
        XtManageChild(widget);
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding = linux::openmotif::text_edit_bindings
                            .object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            linux::openmotif::wnd_bindings.unregister_by_handle(
                binding->widget);
            XtDestroyWidget(binding->widget);
            linux::openmotif::text_edit_bindings
                .unregister_by_handle(self);
            delete binding;
        }
    }

    std::string text_edit::selected_text() const {
        auto *binding = linux::openmotif::text_edit_bindings
                            .object_from_handle(
                                const_cast<text_edit *>(this));
        if (!binding)
            return {};
        char *selection =
            binding->multiline
                ? XmTextGetSelection(binding->widget)
                : XmTextFieldGetSelection(binding->widget);
        const std::string result = selection ? selection : "";
        if (selection)
            XtFree(selection);
        return result;
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding = linux::openmotif::text_edit_bindings
                            .object_from_handle(this);
        if (!binding || _read_only)
            return false;
        XmTextPosition begin = binding->multiline
                                   ? XmTextGetInsertionPosition(
                                         binding->widget)
                                   : XmTextFieldGetInsertionPosition(
                                         binding->widget);
        XmTextPosition end = begin;
        if (binding->multiline) {
            XmTextGetSelectionPosition(binding->widget, &begin, &end);
        } else {
            XmTextFieldGetSelectionPosition(
                binding->widget, &begin, &end);
        }
        std::string candidate = get_text();
        const std::size_t start = byte_offset(candidate, begin);
        const std::size_t finish = byte_offset(candidate, end);
        candidate.replace(start,
                          finish - start,
                          text);
        if (!validate(candidate))
            return false;
        binding->suppress = true;
        if (binding->multiline) {
            XmTextReplace(binding->widget,
                          begin,
                          end,
                          const_cast<char *>(text.c_str()));
            XmTextSetInsertionPosition(
                binding->widget, begin + character_count(text));
        } else {
            XmTextFieldReplace(binding->widget,
                               begin,
                               end,
                               const_cast<char *>(text.c_str()));
            XmTextFieldSetInsertionPosition(
                binding->widget, begin + character_count(text));
        }
        binding->suppress = false;
        return on_native_text(candidate);
    }

    void text_edit::select_all_native() const {
        auto *binding = linux::openmotif::text_edit_bindings
                            .object_from_handle(
                                const_cast<text_edit *>(this));
        if (!binding)
            return;
        const XmTextPosition end = character_count(get_text());
        if (binding->multiline)
            XmTextSetSelection(binding->widget, 0, end, CurrentTime);
        else
            XmTextFieldSetSelection(
                binding->widget, 0, end, CurrentTime);
    }
} // namespace native
