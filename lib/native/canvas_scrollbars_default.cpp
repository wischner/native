//
// Adapts canvas scroll state to platform-owned window scrollbars.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "canvas_scrollbars.h"

namespace native::detail
{
    bool canvas_window_scrollbars(const canvas &) { return false; }
    void synchronize_canvas_scrollbars(canvas &) {}
}
