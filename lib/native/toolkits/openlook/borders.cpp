//
// Applies edge visibility after XView paints windowless Panel items.
// Collection hosts use the shared border drawing stages instead.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../platforms/linux/x_image.h"
#include <algorithm>

namespace
{
    int border_panel_key() {
        static const int key = xv_unique_key();
        return key;
    }

    Notify_value border_event(Notify_client client, Notify_event event,
                               Notify_arg argument, Notify_event_type type) {
        const auto result = notify_next_event_func(client, event, argument, type);
        const auto panel = static_cast<Panel>(xv_get(client, XV_KEY_DATA, border_panel_key()));
        if (panel) linux::openlook::mask_panel_borders(panel);
        return result;
    }
}

namespace linux::openlook
{
    void mask_panel_borders(Panel panel) {
        if (!panel || !cached_display) return;
        const auto window = static_cast<Window>(xv_get(panel, XV_XID));
        if (!window) return;
        for (auto item = static_cast<Panel_item>(xv_get(panel, PANEL_FIRST_ITEM));
             item; item = static_cast<Panel_item>(xv_get(item, PANEL_NEXT_ITEM))) {
            auto *owner = wnd_bindings.object_from_handle(item);
            // The choice-stack menu mark is content, not an outer frame.
            if (auto *combo = dynamic_cast<native::combo_box *>(owner))
                if (combo->get_style() == native::combo_box_style::drop_down_list) continue;
            if (!owner || !owner->get_created() || !owner->get_visible() ||
                owner->get_border_sides() == native::border_sides::all ||
                !(dynamic_cast<native::button *>(owner) ||
                  dynamic_cast<native::combo_box *>(owner) ||
                  dynamic_cast<native::list *>(owner))) continue;
            const auto *bounds = reinterpret_cast<const Rect *>(xv_get(item, PANEL_ITEM_RECT));
            if (!bounds) continue;
            // Panel items are windowless. Read their host's actual paper
            // instead of attempting to construct a drawable for the item.
            const auto color = static_cast<unsigned long>(xv_get(panel, WIN_BACKGROUND_COLOR));
            GC gc = XCreateGC(cached_display, window, 0, nullptr);
            const auto pixels = reinterpret_cast<unsigned long *>(xv_get(panel, WIN_X_COLOR_INDICES));
            XSetForeground(cached_display, gc, pixels ? pixels[color] : WhitePixel(
                cached_display, DefaultScreen(cached_display)));
            const int width = std::max(0, int(bounds->r_width));
            const int height = std::max(0, int(bounds->r_height));
            const int edge_x = std::min(3, width), edge_y = std::min(3, height);
            const auto sides = owner->get_border_sides();
            const auto fill = [&](int x, int y, int w, int h) {
                XFillRectangle(cached_display, window, gc, x, y, w, h);
            };
            if (!native::has_border(sides, native::border_sides::top))
                fill(bounds->r_left, bounds->r_top, width, edge_y);
            if (!native::has_border(sides, native::border_sides::bottom))
                fill(bounds->r_left, bounds->r_top + height - edge_y, width, edge_y);
            if (!native::has_border(sides, native::border_sides::left))
                fill(bounds->r_left, bounds->r_top, edge_x, height);
            if (!native::has_border(sides, native::border_sides::right))
                fill(bounds->r_left + width - edge_x, bounds->r_top, edge_x, height);
            XFreeGC(cached_display, gc);
        }
    }
}

namespace native
{
    void wnd::apply_border_sides() {
        const auto item = linux::openlook::wnd_bindings.handle_from_object(this);
        if (!item) return;
        if (auto *text = dynamic_cast<text_edit *>(this)) {
            if (text->get_mode() == text_edit_mode::single_line)
                xv_set(item, PANEL_VALUE_UNDERLINED,
                    has_border(get_border_sides(), border_sides::bottom), nullptr);
            return;
        }
        if (detail::peer_state<linux::openlook::openlook_collection>(*this) ||
            dynamic_cast<app_wnd *>(this) || dynamic_cast<panel *>(this)) return;
        const auto panel = linux::openlook::parent_panel(this);
        if (!panel) return;
        const Xv_opaque targets[] = {panel, xv_get(panel, CANVAS_NTH_PAINT_WINDOW, 0)};
        for (const auto target : targets) {
            if (!target || xv_get(target, XV_KEY_DATA, border_panel_key())) continue;
            xv_set(target, XV_KEY_DATA, border_panel_key(), panel, nullptr);
            notify_interpose_event_func(target,
                reinterpret_cast<Notify_func>(border_event), NOTIFY_SAFE);
        }
        linux::openlook::mask_panel_borders(panel);
    }
}
