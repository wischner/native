//
// Declares independent sides of a control's outer border. These flags
// do not affect content separators, indicators, or window-manager frames.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <cstdint>

namespace native
{
    enum class border_sides : std::uint8_t
    {
        none = 0,
        top = 1,
        right = 2,
        bottom = 4,
        left = 8,
        all = 15
    };

    // Combine sides; this tiny constexpr operator is intentionally inline.
    constexpr border_sides operator|(border_sides a, border_sides b) {
        return static_cast<border_sides>(unsigned(a) | unsigned(b));
    }

    // Intersect sides; this tiny constexpr operator is intentionally inline.
    constexpr border_sides operator&(border_sides a, border_sides b) {
        return static_cast<border_sides>(unsigned(a) & unsigned(b));
    }

    // Test a side in a mask; intentionally inline for flag expressions.
    constexpr bool has_border(border_sides sides, border_sides side) {
        return (sides & side) != border_sides::none;
    }
}
