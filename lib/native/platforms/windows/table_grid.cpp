//
// Adds consistent grid edges to materialized and virtual ListViews.
// Windows still owns cells, headers, groups and selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "table_grid.h"

#include <commctrl.h>

namespace windows
{
    bool needs_table_grid(const native::table_view &table) {
        return table.get_grid_lines() != native::table_grid_lines::none;
    }

    void draw_table_grid(const native::table_view &table, HWND window,
                         HDC dc, int item) {
        if (!dc || !needs_table_grid(table))
            return;
        RECT row{};
        RECT client{};
        if (!ListView_GetItemRect(window, item, &row, LVIR_BOUNDS) ||
            !GetClientRect(window, &client))
            return;
        HWND header = ListView_GetHeader(window);
        const int saved = SaveDC(dc);
        if (!saved)
            return;
        HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
        if (!pen) {
            RestoreDC(dc, saved);
            return;
        }
        IntersectClipRect(dc, client.left, row.top,
                          client.right, row.bottom);
        HGDIOBJ previous = SelectObject(dc, pen);
        const auto lines = table.get_grid_lines();
        if (lines == native::table_grid_lines::horizontal ||
            lines == native::table_grid_lines::both) {
            MoveToEx(dc, row.left, row.bottom - 1, nullptr);
            LineTo(dc, row.right, row.bottom - 1);
        }
        if (lines == native::table_grid_lines::vertical ||
            lines == native::table_grid_lines::both) {
            for (int column = 0; column < Header_GetItemCount(header);
                 ++column) {
                RECT cell{};
                if (!Header_GetItemRect(header, column, &cell))
                    continue;
                MapWindowPoints(header, window,
                                reinterpret_cast<POINT *>(&cell), 2);
                MoveToEx(dc, cell.right - 1, row.top, nullptr);
                LineTo(dc, cell.right - 1, row.bottom);
            }
        }
        SelectObject(dc, previous);
        DeleteObject(pen);
        RestoreDC(dc, saved);
    }
}
