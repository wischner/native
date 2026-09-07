//
// Shares GEM stock-font text encoding and bitmap glyphs with image
// targets and the scrollbar theme painter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>

namespace linux::gemix
{
    // Convert UTF-8 to printable GEM bytes, substituting unsupported scalars.
    std::string stock_text(const std::string &text);

    // Paint stock glyphs from GEM's font resource; false means unavailable.
    bool draw_stock_text(const native::img &image, const native::rect &clip,
                         const std::string &text, native::point position,
                         native::rgba color);

    // Draw an AES arrow glyph in the current ink, clipped inside its frame.
    // Uses the stock font, or the same built-in glyph if it is unavailable.
    void draw_stock_arrow(native::gpx &graphics, const native::rect &bounds,
                          native::scrollbar_orientation axis,
                          native::scrollbar_part part);
}
