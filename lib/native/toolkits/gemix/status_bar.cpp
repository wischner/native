//
// Chooses status-bar docking within GEM's normalized client rectangle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../status_bar_peer.h"
#include <native/canvas.h>

namespace native::detail
{
    rect status_bar_bounds(const wnd &, const rect &bounds) {
        // work_rect() excludes the complete AES frame. No additional
        // corner cutout is needed for the status strip.
        return bounds;
    }

    wnd &status_bar_host(wnd &, canvas &content) { return content; }
}
