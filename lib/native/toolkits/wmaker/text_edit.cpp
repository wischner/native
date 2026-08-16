//
// Implements WINGs single-line and multiline text controls with live
// complete-value validation and portable clipboard commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include <X11/keysym.h>
#include <WINGs/WINGs.h>

#include <native/text_edit.h>

#include "globals.h"

namespace
{
    // WINGs exposes selection mutation but not selection inspection for
    // WMTextField. This prefix mirrors the pinned WINGs ABI through the
    // selection member only; no ownership crosses the backend boundary.
    struct text_field_prefix
    {
        int widget_class = 0;
        WMView *view = nullptr;
        char *text = nullptr;
        int text_length = 0;
        int buffer_size = 0;
        int view_position = 0;
        int cursor_position = 0;
        short usable_width = 0;
        short offset_width = 0;
        WMRange selection = {};
    };

    linux::wmaker::native_text_edit *binding_for(
        native::text_edit *owner) {
        return linux::wmaker::text_edit_bindings
            .object_from_handle(owner);
    }

    std::string current_text(
        const linux::wmaker::native_text_edit *binding) {
        if (!binding)
            return {};
        if (binding->field) {
            char *value = WMGetTextFieldText(binding->field);
            std::string result = value ? value : "";
            if (value)
                std::free(value);
            return result;
        }
        if (binding->text) {
            char *value = WMGetTextStream(binding->text);
            std::string result = value ? value : "";
            if (value)
                std::free(value);
            return result;
        }
        return {};
    }

    void set_native_text(
        linux::wmaker::native_text_edit *binding,
        const std::string &text) {
        if (binding->field) {
            WMSetTextFieldText(binding->field, text.c_str());
        } else if (binding->text) {
            WMFreezeText(binding->text);
            WMClearText(binding->text);
            if (!text.empty())
                WMAppendTextStream(binding->text, text.c_str());
            WMThawText(binding->text);
        }
    }

    void synchronize(native::text_edit *owner) {
        auto *binding = binding_for(owner);
        if (!owner || !binding || binding->suppress)
            return;
        const std::string candidate = current_text(binding);
        if (owner->on_native_text(candidate))
            return;
        binding->suppress = true;
        set_native_text(binding, owner->get_text());
        binding->suppress = false;
    }

    void field_changed(WMTextFieldDelegate *delegate,
                       WMNotification *) {
        auto *owner = delegate
                          ? static_cast<native::text_edit *>(
                                delegate->data)
                          : nullptr;
        if (owner) {
            linux::wmaker::defer([owner]() {
                if (owner->get_created())
                    synchronize(owner);
            });
        }
    }

    bool control_shortcut(native::text_edit *owner,
                          const XEvent *event) {
        if (!owner || !event || event->type != KeyPress ||
            (event->xkey.state & ControlMask) == 0) {
            return false;
        }
        XKeyEvent key_event = event->xkey;
        KeySym symbol = XLookupKeysym(&key_event, 0);
        switch (symbol) {
        case XK_a:
        case XK_A:
            owner->select_all();
            return true;
        case XK_c:
        case XK_C:
            owner->copy();
            return true;
        case XK_x:
        case XK_X:
            owner->cut();
            return true;
        case XK_v:
        case XK_V:
            owner->paste();
            return true;
        default:
            return false;
        }
    }

    void handle_text_event(XEvent *event, void *client_data) {
        auto *owner = static_cast<native::text_edit *>(client_data);
        if (!owner || !event)
            return;
        if ((event->type == KeyPress ||
             event->type == ButtonPress) &&
            !linux::wmaker::permit_input(owner)) {
            return;
        }
        if (event->type == KeyPress &&
            (event->xkey.state & ControlMask) != 0) {
            const XEvent copy = *event;
            linux::wmaker::defer([owner, copy]() mutable {
                if (owner->get_created())
                    control_shortcut(owner, &copy);
            });
            return;
        }
        if (event->type == KeyPress ||
            event->type == ButtonRelease) {
            linux::wmaker::defer([owner]() {
                if (owner->get_created())
                    synchronize(owner);
            });
        }
    }
} // namespace

namespace native
{
    void text_edit::apply_text() {
        auto *binding = binding_for(this);
        if (!binding) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing text-edit binding.");
        }
        binding->suppress = true;
        set_native_text(binding, _text);
        binding->suppress = false;
    }

    void text_edit::apply_read_only() {
        auto *binding = binding_for(this);
        if (!binding) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing text-edit binding.");
        }
        if (binding->field)
            WMSetTextFieldEditable(binding->field, !_read_only);
        else if (binding->text)
            WMSetTextEditable(binding->text, !_read_only);
    }

    void text_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding = new linux::wmaker::native_text_edit;
        WMWidget *parent = linux::wmaker::parent_widget(self);
        if (_mode == text_edit_mode::single_line) {
            binding->field = WMCreateTextField(parent);
            binding->widget = binding->field;
            binding->delegate.data = self;
            binding->delegate.didChange = field_changed;
            WMSetTextFieldDelegate(
                binding->field, &binding->delegate);
            WMSetTextFieldText(binding->field, _text.c_str());
            WMSetTextFieldEditable(binding->field, !_read_only);
        } else {
            binding->text = WMCreateText(parent);
            binding->widget = binding->text;
            WMSetTextHasVerticalScroller(binding->text, True);
            WMSetTextEditable(binding->text, !_read_only);
            if (!_text.empty())
                WMAppendTextStream(binding->text, _text.c_str());
        }
        if (!binding->widget) {
            delete binding;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create text edit.");
        }

        const point position =
            linux::wmaker::control_position(self);
        WMMoveWidget(binding->widget, position.x, position.y);
        WMResizeWidget(binding->widget, _bounds.d.w, _bounds.d.h);
        WMCreateEventHandler(
            WMWidgetView(binding->widget),
            KeyPressMask | ButtonPressMask | ButtonReleaseMask,
            handle_text_event,
            self);
        try {
            linux::wmaker::wnd_bindings.register_pair(
                binding->widget, self);
            linux::wmaker::text_edit_bindings.register_pair(
                self, binding);
        } catch (...) {
            linux::wmaker::wnd_bindings.unregister_by_handle(
                binding->widget);
            WMDestroyWidget(binding->widget);
            delete binding;
            throw;
        }
        _created = true;
        self->on_wnd_create.emit();
    }

    void text_edit::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated text "
                "edit.");
        }
        auto *binding = binding_for(
            const_cast<text_edit *>(this));
        if (!binding || !binding->widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing text-edit binding.");
        }
        WMMapWidget(binding->widget);
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding = binding_for(self);
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        linux::wmaker::text_edit_bindings.unregister_by_handle(self);
        if (binding && binding->widget)
            WMDestroyWidget(binding->widget);
        delete binding;
    }

    std::string text_edit::selected_text() const {
        auto *binding = binding_for(
            const_cast<text_edit *>(this));
        if (!binding)
            return {};
        if (binding->text) {
            char *value = WMGetTextSelectedStream(binding->text);
            std::string result = value ? value : "";
            if (value)
                std::free(value);
            return result;
        }
        if (!binding->field)
            return {};
        auto *field = reinterpret_cast<text_field_prefix *>(
            binding->field);
        const int begin = std::clamp(
            field->selection.position, 0, field->text_length);
        const int count = std::clamp(
            field->selection.count, 0, field->text_length - begin);
        return std::string(field->text + begin,
                           static_cast<std::size_t>(count));
    }

    bool text_edit::replace_selected_text(
        const std::string &replacement) {
        auto *binding = binding_for(this);
        if (!binding || _read_only)
            return false;
        if (binding->field) {
            auto *field = reinterpret_cast<text_field_prefix *>(
                binding->field);
            const int begin = std::clamp(
                field->selection.position, 0, field->text_length);
            const int count = std::clamp(
                field->selection.count, 0, field->text_length - begin);
            if (count == 0)
                return false;
            std::string candidate = _text;
            candidate.replace(static_cast<std::size_t>(begin),
                              static_cast<std::size_t>(count),
                              replacement);
            if (!validate(candidate))
                return false;
            binding->suppress = true;
            WMSetTextFieldText(binding->field, candidate.c_str());
            WMSetTextFieldCursorPosition(
                binding->field,
                static_cast<unsigned int>(begin +
                                          replacement.size()));
            binding->suppress = false;
            return on_native_text(candidate);
        }

        const std::string previous = _text;
        std::string mutable_replacement = replacement;
        binding->suppress = true;
        const Bool replaced = WMReplaceTextSelection(
            binding->text,
            mutable_replacement.empty()
                ? nullptr
                : mutable_replacement.data());
        const std::string candidate = current_text(binding);
        if (!replaced || !validate(candidate)) {
            set_native_text(binding, previous);
            binding->suppress = false;
            return false;
        }
        binding->suppress = false;
        return on_native_text(candidate);
    }

    void text_edit::select_all_native() const {
        auto *binding = binding_for(
            const_cast<text_edit *>(this));
        if (!binding)
            return;
        if (binding->field) {
            WMSelectTextFieldRange(
                binding->field,
                WMRange{0, static_cast<int>(_text.size())});
        } else if (binding->text && !_text.empty()) {
            WMFindInTextStream(binding->text,
                               _text.c_str(),
                               True,
                               True);
        }
    }
} // namespace native
