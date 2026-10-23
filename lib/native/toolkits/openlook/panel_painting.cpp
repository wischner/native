//
// Preserves native, windowless XView items while copying a portable
// background. Item painting remains XView's responsibility, so a ruler
// update cannot erase an editor or flash its selection/caret.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "panel_painting.h"
#include <X11/Xutil.h>
#include <algorithm>
namespace linux::openlook
{
    void merge_damage(native::rect &pending, const native::rect &area) {
        if (!area.w() || !area.h())
            return;
        if (!pending.w() || !pending.h()) {
            pending = area;
            return;
        }
        const int x = std::min(pending.x1(), area.x1());
        const int y = std::min(pending.y1(), area.y1());
        pending = native::rect(x,
                               y,
                               std::max(pending.x2(), area.x2()) - x,
                               std::max(pending.y2(), area.y2()) - y);
    }
    void copy_panel_background(Panel panel,
                               Window target,
                               openlook_gpx &cache,
                               const native::rect &area,
                               bool preserve_items) {
        if (!area.w() || !area.h())
            return;
        Region region = XCreateRegion();
        XRectangle damage{area.x1(), area.y1(), area.w(), area.h()};
        XUnionRectWithRegion(&damage, region, region);
        for (auto item = static_cast<Panel_item>(
                 xv_get(panel, PANEL_FIRST_ITEM));
             item;
             item = static_cast<Panel_item>(
                 xv_get(item, PANEL_NEXT_ITEM))) {
            if (!preserve_items || !xv_get(item, XV_SHOW))
                continue;
            const auto *bounds =
                reinterpret_cast<Rect *>(xv_get(item, XV_RECT));
            if (!bounds || bounds->r_width <= 0 ||
                bounds->r_height <= 0)
                continue;
            Region occupied = XCreateRegion();
            XRectangle rectangle{
                bounds->r_left,
                bounds->r_top,
                static_cast<unsigned short>(bounds->r_width),
                static_cast<unsigned short>(bounds->r_height)};
            XUnionRectWithRegion(&rectangle, occupied, occupied);
            XSubtractRegion(region, occupied, region);
            XDestroyRegion(occupied);
        }
        XSetRegion(cached_display, cache.gc, region);
        XCopyArea(cached_display,
                  cache.backbuffer,
                  target,
                  cache.gc,
                  area.x1(),
                  area.y1(),
                  area.w(),
                  area.h(),
                  area.x1(),
                  area.y1());
        XSetClipMask(cached_display, cache.gc, None);
        XDestroyRegion(region);
    }
} // namespace linux::openlook
