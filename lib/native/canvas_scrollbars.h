//
// Adapts canvas scroll state to platform-owned window scrollbars.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

namespace native { class canvas; }
namespace native::detail
{
    // True when the enclosing native frame owns the scrollbar geometry.
    bool canvas_window_scrollbars(const canvas &content);
    // Publish the current content range, viewport, and clamped position.
    void synchronize_canvas_scrollbars(canvas &content);
}
