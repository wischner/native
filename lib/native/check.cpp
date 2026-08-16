//
// Implements backend-neutral check state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <utility>

#include <native.h>
#include <native/check.h>

namespace native
{
    check::check(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    check::check(const std::string &text,
                 const point &position,
                 const size &dimensions)
        : check(text,
                position.x,
                position.y,
                dimensions.w,
                dimensions.h) {}

    check::check(const std::string &text, const rect &bounds)
        : check(text, bounds.p, bounds.d) {}

    check::~check() {
        destroy();
    }

    const std::string &check::get_text() const {
        return _text;
    }

    check &check::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    bool check::get_checked() const {
        return _checked;
    }

    check &check::set_checked(bool checked) {
        if (_checked == checked)
            return *this;
        _checked = checked;
        if (_created)
            apply_checked();
        return *this;
    }

    void check::on_native_checked(bool checked) {
        if (_checked == checked)
            return;
        _checked = checked;
        on_change.emit(_checked);
    }
} // namespace native
