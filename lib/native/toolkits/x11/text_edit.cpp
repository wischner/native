//
// Implements native Athena text editors with complete-value validation,
// portable clipboard commands, and standard editing shortcuts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/keysym.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    Widget parent_widget(native::text_edit *editor) {
        native::wnd *parent = editor->get_parent();
        Widget widget = parent
                            ? linux::x11::wnd_bindings
                                  .handle_from_object(parent)
                            : nullptr;
        if (!parent || !parent->get_created() || !widget) {
            throw std::runtime_error(
                "X11/Athena: text_edit requires a created parent.");
        }
        return widget;
    }

    std::string widget_text(Widget widget) {
        char *value = nullptr;
        XtVaGetValues(widget, XtNstring, &value, nullptr);
        return value ? value : "";
    }

    std::size_t boundary_before(const std::string &text,
                                std::size_t offset) {
        offset = std::min(offset, text.size());
        if (offset < text.size() &&
            (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80) {
            return native::detail::previous_utf8(text, offset);
        }
        return offset;
    }

    std::size_t boundary_after(const std::string &text,
                               std::size_t offset) {
        offset = std::min(offset, text.size());
        while (offset < text.size() &&
               (static_cast<unsigned char>(text[offset]) & 0xc0) ==
                   0x80) {
            ++offset;
        }
        return offset;
    }

    void source_changed(Widget,
                        XtPointer client_data,
                        XtPointer) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        auto *binding = owner
                            ? linux::x11::text_edit_bindings
                                  .object_from_handle(owner)
                            : nullptr;
        if (!owner || !binding || binding->suppress)
            return;
        const std::string candidate = widget_text(binding->widget);
        if (owner->on_native_text(candidate))
            return;
        binding->suppress = true;
        XtVaSetValues(binding->widget,
                      XtNstring,
                      owner->get_text().c_str(),
                      nullptr);
        binding->suppress = false;
    }

    void key_pressed(Widget widget,
                     XtPointer client_data,
                     XEvent *event,
                     Boolean *continue_dispatch) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        if (!owner || !event || event->type != KeyPress)
            return;
        KeySym symbol = NoSymbol;
        char buffer[64] = {};
        const int length = XLookupString(&event->xkey,
                                         buffer,
                                         sizeof(buffer),
                                         &symbol,
                                         nullptr);

        if ((event->xkey.state & ControlMask) == 0) {
            XawTextPosition begin = 0;
            XawTextPosition end = 0;
            XawTextGetSelectionPos(widget, &begin, &end);
            if (begin >= end || owner->get_read_only())
                return;

            std::string replacement;
            bool replace = false;
            if (symbol == XK_BackSpace || symbol == XK_Delete) {
                replace = true;
            } else if (length > 0 &&
                       (event->xkey.state & Mod1Mask) == 0) {
                replacement.assign(
                    buffer, static_cast<std::size_t>(length));
                replace = true;
            }
            if (!replace)
                return;

            const std::string current = widget_text(widget);
            begin = static_cast<XawTextPosition>(boundary_before(
                current, static_cast<std::size_t>(begin)));
            end = static_cast<XawTextPosition>(boundary_after(
                current, static_cast<std::size_t>(end)));
            std::string candidate = current;
            candidate.replace(static_cast<std::size_t>(begin),
                              static_cast<std::size_t>(end - begin),
                              replacement);
            if (!owner->validate(candidate)) {
                if (continue_dispatch)
                    *continue_dispatch = False;
                return;
            }

            XawTextBlock block{};
            block.firstPos = 0;
            block.length = static_cast<int>(replacement.size());
            block.ptr = replacement.data();
            block.format = FMT8BIT;
            auto *binding = linux::x11::text_edit_bindings
                                .object_from_handle(owner);
            if (!binding)
                return;
            binding->suppress = true;
            const int result = XawTextReplace(
                widget, begin, end, &block);
            binding->suppress = false;
            if (result == XawEditDone) {
                XawTextSetInsertionPoint(
                    widget, begin + block.length);
                owner->on_native_text(candidate);
            }
            if (continue_dispatch)
                *continue_dispatch = False;
            return;
        }

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
        auto *binding =
            linux::x11::text_edit_bindings.object_from_handle(this);
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: Missing text-edit widget.");
        binding->suppress = true;
        XtVaSetValues(binding->widget,
                      XtNstring,
                      _text.c_str(),
                      nullptr);
        binding->suppress = false;
    }

    void text_edit::apply_read_only() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Missing text-edit widget.");
        XtVaSetValues(widget,
                      XtNeditType,
                      _read_only ? XawtextRead : XawtextEdit,
                      nullptr);
    }

    void text_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        Widget widget = XtVaCreateWidget(
            "textEdit",
            asciiTextWidgetClass,
            parent_widget(self),
            XtNhorizDistance,
            _bounds.p.x,
            XtNvertDistance,
            _bounds.p.y,
            XtNwidth,
            _bounds.d.w,
            XtNheight,
            _bounds.d.h,
            XtNstring,
            _text.c_str(),
            XtNeditType,
            _read_only ? XawtextRead : XawtextEdit,
            XtNscrollVertical,
            _mode == text_edit_mode::multi_line
                ? XawtextScrollWhenNeeded
                : XawtextScrollNever,
            XtNwrap,
            _mode == text_edit_mode::multi_line ? XawtextWrapWord
                                                : XawtextWrapNever,
            XtNleft,
            XtChainLeft,
            XtNright,
            XtChainLeft,
            XtNtop,
            XtChainTop,
            XtNbottom,
            XtChainTop,
            nullptr);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Failed to create text_edit.");

        auto *binding = new linux::x11::xaw_text_edit;
        binding->widget = widget;
        binding->source = XawTextGetSource(widget);
        linux::x11::wnd_bindings.register_pair(widget, self);
        linux::x11::text_edit_bindings.register_pair(self, binding);
        XtAddCallback(binding->source,
                      XtNcallback,
                      source_changed,
                      self);
        XtAddEventHandler(widget,
                          KeyPressMask,
                          False,
                          key_pressed,
                          self);
        _created = true;
        self->on_wnd_create.emit();
    }

    void text_edit::show() const {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: text_edit is not created.");
        XtManageChild(widget);
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding =
            linux::x11::text_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            linux::x11::wnd_bindings.unregister_by_handle(
                binding->widget);
            XtDestroyWidget(binding->widget);
            linux::x11::text_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }

    std::string text_edit::selected_text() const {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (!widget)
            return {};
        XawTextPosition begin = 0;
        XawTextPosition end = 0;
        XawTextGetSelectionPos(widget, &begin, &end);
        const std::string value = widget_text(widget);
        if (begin < 0 || begin >= end ||
            static_cast<std::size_t>(end) > value.size())
            return {};
        const std::size_t first = boundary_before(
            value, static_cast<std::size_t>(begin));
        const std::size_t last = boundary_after(
            value, static_cast<std::size_t>(end));
        return value.substr(first, last - first);
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget || _read_only)
            return false;
        XawTextPosition begin = XawTextGetInsertionPoint(widget);
        XawTextPosition end = begin;
        XawTextGetSelectionPos(widget, &begin, &end);
        const std::string current = widget_text(widget);
        begin = static_cast<XawTextPosition>(boundary_before(
            current, static_cast<std::size_t>(begin)));
        end = static_cast<XawTextPosition>(boundary_after(
            current, static_cast<std::size_t>(end)));
        std::string candidate = current;
        candidate.replace(static_cast<std::size_t>(begin),
                          static_cast<std::size_t>(end - begin),
                          text);
        if (!validate(candidate))
            return false;
        XawTextBlock block{};
        block.firstPos = 0;
        block.length = static_cast<int>(text.size());
        block.ptr = const_cast<char *>(text.data());
        block.format = FMT8BIT;
        auto *binding =
            linux::x11::text_edit_bindings.object_from_handle(this);
        binding->suppress = true;
        const int result = XawTextReplace(widget, begin, end, &block);
        binding->suppress = false;
        if (result != XawEditDone)
            return false;
        XawTextSetInsertionPoint(widget, begin + block.length);
        return on_native_text(candidate);
    }

    void text_edit::select_all_native() const {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (widget) {
            XawTextSetSelection(widget,
                                0,
                                static_cast<XawTextPosition>(
                                    widget_text(widget).size()));
        }
    }
} // namespace native
