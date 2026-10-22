//
// Resolves single-line input geometry from the active backend's text-field
// metrics without changing native widget selection or drawing ownership.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/geometry.h>
namespace native
{
    class wnd;
    namespace detail
    {
        // Keep width and horizontal placement; center a standard-height field
        // within extra vertical space. Zero-height fields remain empty.
        rect input_bounds(const wnd &owner, const rect &requested);
    }
}
