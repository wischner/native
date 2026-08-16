//
// Declares the bitmap-font fallback used by graphics targets that do
// not provide a usable native text renderer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/graphics.h>

namespace native::detail
{
    // Draw fallback bitmap text from the supplied line baseline.
    void draw_bitmap_text(const img &target,
                          const rect &clip,
                          const std::string &text,
                          point baseline,
                          rgba color,
                          int advance,
                          int line_height);
} // namespace native::detail
