//
// Chooses the docking host for a scrolling document's status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "status_bar_peer.h"

namespace native::detail
{
    rect status_bar_bounds(const wnd &, const rect &bounds) { return bounds; }

    wnd &status_bar_host(wnd &owner, canvas &) { return owner; }
}
