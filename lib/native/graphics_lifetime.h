//
// Gives private native paint guards access to drawer lifetime tokens.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/graphics.h>
namespace native::detail
{
    class graphics_access
    {
    public:
        // Capture liveness without retaining a dying window's native drawer.
        static std::weak_ptr<void> lifetime(gpx &graphics);
    };
} // namespace native::detail
