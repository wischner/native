//
// Paints centered single-pixel rules in otherwise native XView menus.
// Reads XView's resolved row geometry after its normal popup repaint.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"

// XView's private C menu layout has a member named `class`.
#define class xview_menu_class
#include <xview_private/om_impl.h>
#undef class

namespace
{
    int separator_key() {
        static const int key = xv_unique_key();
        return key;
    }

    int paint_hook_key() {
        static const int key = xv_unique_key();
        return key;
    }

    // Cached popup windows can be reused by a different menu. Always
    // recover their current menu and paint only our tagged separators.
    Notify_value paint_separators(Notify_client client, Notify_event input,
                                 Notify_arg argument, Notify_event_type type) {
        const auto result = notify_next_event_func(client, input, argument, type);
        const auto *event = reinterpret_cast<Event *>(input);
        if (!event || event_id(event) != WIN_REPAINT)
            return result;
        const auto *menu = reinterpret_cast<Xv_menu_info *>(
            xv_get(client, XV_KEY_DATA, MENU_WINDOW_MENU));
        if (!menu || !menu->active || !menu->group_info ||
            menu->nrows <= 0 || menu->ncols <= 0)
            return result;
        Display *display = linux::openlook::cached_display;
        const Window window = xv_get(client, XV_XID);
        if (!display || !window) return result;
        const int border = menu->xview_menu_class == MENU_COMMAND ? 2 : 6;
        const int row_height = menu->default_image.height;
        const int column_width = (menu->menurect.r_width - 2 * border) /
                                 menu->ncols;
        GC graphics = nullptr;
        for (int i = 0; i < menu->nitems; ++i) {
            if (!xv_get(MENU_ITEM_PUBLIC(menu->item_list[i]),
                        XV_KEY_DATA, separator_key()))
                continue;
            if (!graphics) {
                graphics = XCreateGC(display, window, 0, nullptr);
                const auto *pixels = reinterpret_cast<unsigned long *>(
                    xv_get(client, WIN_X_COLOR_INDICES));
                const auto foreground = xv_get(client, WIN_FOREGROUND_COLOR);
                XSetForeground(display, graphics, pixels ? pixels[foreground] :
                    BlackPixel(display, DefaultScreen(display)));
            }
            const int row = menu->column_major ? i % menu->nrows : i / menu->ncols;
            const int column = menu->column_major ? i / menu->nrows : i % menu->ncols;
            const int left = border + column * column_width;
            const int y = border + row * row_height + (row_height - 1) / 2;
            XDrawLine(display, window, graphics,
                      left + 1, y, left + column_width - 2, y);
        }
        if (graphics) XFreeGC(display, graphics);
        return result;
    }

    // XView creates its popup window before generating individual items.
    // Install once per cached window, before its first exposure is painted.
    Menu_item prepare_separator(Menu_item item, Menu_generate operation) {
        if (operation != MENU_DISPLAY) return item;
        const auto parent = xv_get(item, MENU_PARENT);
        auto *menu = parent ? MENU_PRIVATE(parent) : nullptr;
        if (menu && menu->window &&
            !xv_get(menu->window, XV_KEY_DATA, paint_hook_key())) {
            notify_interpose_event_func(menu->window,
                reinterpret_cast<Notify_func>(paint_separators), NOTIFY_SAFE);
            notify_interpose_event_func(menu->window,
                reinterpret_cast<Notify_func>(paint_separators), NOTIFY_IMMEDIATE);
            xv_set(menu->window, XV_KEY_DATA, paint_hook_key(), TRUE, nullptr);
        }
        return item;
    }
}

namespace linux::openlook
{
    Menu_item create_menu_separator() {
        return xv_create(XV_NULL, MENUITEM, MENU_RELEASE,
            MENU_STRING, " ", MENU_INACTIVE, TRUE, MENU_FEEDBACK, FALSE,
            MENU_GEN_PROC, prepare_separator,
            XV_KEY_DATA, separator_key(), TRUE, nullptr);
    }
}
