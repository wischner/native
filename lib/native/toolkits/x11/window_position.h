//
// Declares X11 top-level shell placement constrained to the active
// monitor work area and the window manager's frame dimensions.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <X11/Intrinsic.h>

#include <native/geometry.h>

namespace linux::x11
{
    // Ask an EWMH window manager to publish shell frame dimensions.
    void request_frame_extents(Widget shell);

    // Keep a preferred client position and its title frame reachable.
    native::point constrain_shell_position(
        Widget shell,
        const native::point &preferred,
        const native::size &dimensions);
} // namespace linux::x11
