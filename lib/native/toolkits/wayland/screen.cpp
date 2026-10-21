//
// Enumerates logical Wayland monitor geometry through GDK.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
namespace native
{
    const std::vector<screen> &screen::detect() {
        linux::wayland::initialize();
        _screens.clear();
        auto *monitors =
            gdk_display_get_monitors(gdk_display_get_default());
        for (guint index = 0;
             index < g_list_model_get_n_items(monitors);
             ++index) {
            auto *monitor =
                GDK_MONITOR(g_list_model_get_item(monitors, index));
            GdkRectangle area;
            gdk_monitor_get_geometry(monitor, &area);
            rect bounds(area.x, area.y, area.width, area.height);
            _screens.emplace_back(index, bounds, bounds, index == 0);
            g_object_unref(monitor);
        }
        normalize();
        return _screens;
    }
} // namespace native
