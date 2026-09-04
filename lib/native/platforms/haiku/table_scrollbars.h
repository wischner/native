//
// Adapts virtual table positions to real Haiku scrollbar children.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

namespace native { class table_view; }

namespace haiku
{
    // Create native bars owned by the table's BView, initially hidden.
    void create_table_scrollbars(native::table_view &table);

    // Synchronize geometry, ranges, visibility and values under the view lock.
    void refresh_table_scrollbars(native::table_view &table);
}
