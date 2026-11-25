//
// Declares square icon fitting shared by native window-icon adapters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <memory>

#include <native/graphics.h>

namespace native::detail
{
    //
    // Fit an icon into a transparent square, preserving aspect ratio.
    //
    // Parameters:
    //      source      - Application icon of any positive size.
    //      edge        - Positive square edge in pixels.
    //
    // Returns:
    //      An independent edge-by-edge image with the scaled source
    //      centered on transparent pixels.
    //
    // Notes:
    //      Whole-number enlargement keeps nearest-pixel edges; every
    //      other scale uses linear sampling.
    //
    std::unique_ptr<img> fit_window_icon(const img &source, dim edge);
} // namespace native::detail
