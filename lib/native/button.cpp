//
// Implements backend-neutral button state and property behavior.
// Backends provide creation, destruction, display, and label updates.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <utility>

#include <native/button.h>

namespace native
{
    button::button(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    button::button(const std::string &text,
                   const point &position,
                   const size &dimensions)
        : button(text,
                 position.x,
                 position.y,
                 dimensions.w,
                 dimensions.h) {}

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p, bounds.d) {}

    button::~button() {
        destroy();
    }

    const std::string &button::get_text() const {
        return _text;
    }

    button &button::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }
} // namespace native
