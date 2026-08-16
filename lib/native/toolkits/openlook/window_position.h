//
// Declares XView top-level placement constrained to an active screen
// work area while keeping the OPEN LOOK title decoration reachable.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

#include <xview/xview.h>
#include <xview/frame.h>

#include "xview_compat.h"

namespace linux::openlook
{
    // Return the outer window-manager frame position on the root.
    native::point frame_position(Frame frame);

    // Ask an EWMH window manager to publish frame dimensions.
    void request_frame_extents(Frame frame);

    // Constrain a preferred XView frame position to a work area.
    native::point constrain_frame_position(
        Frame frame,
        const native::point &preferred,
        const native::size &dimensions);
} // namespace linux::openlook
