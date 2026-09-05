//
// Declares the small grid-line supplement for native report ListViews.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <windows.h>
#include <native/table_view.h>

namespace windows
{
    // Return whether native rows need the requested grid-edge supplement.
    bool needs_table_grid(const native::table_view &table);

    // Add only requested grid edges after Windows has painted a native row.
    void draw_table_grid(const native::table_view &table, HWND window,
                         HDC dc, int item);
}
