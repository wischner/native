//
// Implements portable text-edit state, complete-value validation, and
// clipboard commands shared by native and emulated editor backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <stdexcept>
#include <utility>

#include <native/clipboard.h>

#include "text_util.h"

namespace native
{
    text_edit::text_edit(std::string text,
                         text_edit_mode mode,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text))
        , _mode(mode) {
        if (!validate(_text))
            throw std::invalid_argument(
                "text_edit requires valid text for its mode");
    }

    text_edit::text_edit(const std::string &text,
                         text_edit_mode mode,
                         const point &position,
                         const size &dimensions)
        : text_edit(text,
                    mode,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    text_edit::text_edit(const std::string &text,
                         text_edit_mode mode,
                         const rect &bounds)
        : text_edit(text, mode, bounds.p, bounds.d) {}

    text_edit::~text_edit() {
        destroy();
    }

    const std::string &text_edit::get_text() const {
        return _text;
    }

    text_edit &text_edit::set_text(const std::string &text) {
        if (!validate(text))
            throw std::invalid_argument(
                "text_edit value was rejected by validation");
        if (_text == text)
            return *this;
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    text_edit_mode text_edit::get_mode() const {
        return _mode;
    }

    bool text_edit::get_read_only() const {
        return _read_only;
    }

    text_edit &text_edit::set_read_only(bool read_only) {
        if (_read_only == read_only)
            return *this;
        _read_only = read_only;
        if (_created)
            apply_read_only();
        return *this;
    }

    const text_validator &text_edit::get_validator() const {
        return _validator;
    }

    text_edit &text_edit::set_validator(text_validator validator) {
        if (validator && !validator(_text))
            throw std::invalid_argument(
                "text validator rejects the current value");
        _validator = std::move(validator);
        return *this;
    }

    text_edit &text_edit::clear_validator() {
        _validator = nullptr;
        return *this;
    }

    bool text_edit::validate(const std::string &text) const {
        if (!detail::valid_utf8(text) ||
            text.find('\0') != std::string::npos ||
            text.find('\r') != std::string::npos)
            return false;
        if (_mode == text_edit_mode::single_line &&
            text.find('\n') != std::string::npos)
            return false;
        return !_validator || _validator(text);
    }

    bool text_edit::on_native_text(const std::string &text) {
        if (_read_only || !validate(text))
            return false;
        if (_text == text)
            return true;
        _text = text;
        on_change.emit(_text);
        return true;
    }

    bool text_edit::copy() const {
        const std::string selection = selected_text();
        if (selection.empty())
            return false;
        clipboard output = clipboard::open_write();
        output.write_text(selection).commit();
        return true;
    }

    bool text_edit::cut() {
        if (_read_only)
            return false;
        const std::string selection = selected_text();
        if (selection.empty())
            return false;
        clipboard output = clipboard::open_write();
        output.write_text(selection).commit();
        return replace_selected_text(std::string());
    }

    bool text_edit::paste() {
        if (_read_only)
            return false;
        clipboard input = clipboard::open_read();
        if (!input.has(clipboard_format::text))
            return false;
        return replace_selected_text(input.read_text());
    }

    void text_edit::select_all() const {
        select_all_native();
    }
} // namespace native
