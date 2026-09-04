//
// Provides the out-of-line polymorphic lifetime for the shared collection
// control base.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/collection_view.h>

#include <algorithm>

namespace native
{
    collection_view::~collection_view() = default;

    int collection_view::get_scroll_offset() const {
        return _scroll_offset;
    }

    collection_view &collection_view::set_scroll_offset(int offset) {
        const int clamped = std::clamp(
            offset, 0, maximum_scroll_offset());
        if (_scroll_offset == clamped)
            return *this;
        _scroll_offset = clamped;
        if (_created)
            apply_scroll_offset();
        invalidate();
        return *this;
    }

    int collection_view::maximum_scroll_offset() const {
        return 0;
    }

    void collection_view::apply_scroll_offset() {
        invalidate();
    }
} // namespace native
