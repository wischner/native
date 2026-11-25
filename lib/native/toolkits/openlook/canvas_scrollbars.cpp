//
// Adapts canvas scroll state to XView Scrollbars on the canvas Panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

#include "../../canvas_scrollbars.h"
#include "collection_host.h"

namespace native::detail
{
    // The XView Scrollbars sit inside the canvas, so the portable
    // geometry keeps reserving their tracks.
    bool canvas_window_scrollbars(const canvas &) { return false; }

    void synchronize_canvas_scrollbars(canvas &content) {
        if (content.get_created())
            linux::openlook::synchronize_collection_scrollbars(content);
    }
}
