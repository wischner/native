//
// Implements backend-neutral radio state and grouping.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <utility>

#include <native/radio.h>

namespace native
{
    radio::radio(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    radio::radio(const std::string &text,
                 const point &position,
                 const size &dimensions)
        : radio(text,
                position.x,
                position.y,
                dimensions.w,
                dimensions.h) {}

    radio::radio(const std::string &text, const rect &bounds)
        : radio(text, bounds.p, bounds.d) {}

    radio::~radio() {
        destroy();
    }

    const std::string &radio::get_text() const {
        return _text;
    }

    radio &radio::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    bool radio::get_selected() const {
        return _selected;
    }

    radio &radio::set_selected(bool selected) {
        if (!selected) {
            if (!_selected)
                return *this;
            _selected = false;
            if (_created)
                apply_selected();
            return *this;
        }
        select_exclusive(false);
        return *this;
    }

    void radio::on_native_selected() {
        select_exclusive(true);
    }

    void radio::select_exclusive(bool notify) {
        if (!_parent) {
            const bool changed = !_selected;
            _selected = true;
            if (_created)
                apply_selected();
            if (notify && changed)
                on_change.emit(true);
            return;
        }

        for (wnd *sibling : _parent->_children) {
            auto *other = dynamic_cast<radio *>(sibling);
            if (!other || other == this || !other->_selected) {
                continue;
            }
            other->_selected = false;
            if (other->_created)
                other->apply_selected();
            if (notify)
                other->on_change.emit(false);
        }
        const bool changed = !_selected;
        _selected = true;
        if (_created)
            apply_selected();
        if (notify && changed)
            on_change.emit(true);
    }
} // namespace native
