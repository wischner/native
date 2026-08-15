//
// Implements backend-independent font handle queries.
// Creation, destruction, moves, and stock fonts remain backend-specific
// because those operations own native toolkit resources.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/font.h>

namespace native
{
    bool font_t::valid() const {
        return _id != 0;
    }

    const font_spec &font_t::spec() const {
        return _spec;
    }

    std::uint32_t font_t::id() const {
        return _id;
    }
}
