//
// Chooses the docking host for a scrolling document's status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../status_bar_peer.h"
#include <native/canvas.h>

namespace native::detail
{
    wnd &status_bar_host(wnd &, canvas &content) { return content; }
}
