//
// Declares damage aggregation and background copies that preserve
// windowless XView Panel items during portable painting.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "globals.h"
namespace linux::openlook
{
    // Accumulate invalid rectangles until the next notifier delivery.
    void merge_damage(native::rect &pending, const native::rect &area);
    // Copy background pixels around visible native item rectangles.
    void copy_panel_background(Panel panel,
                               Window target,
                               openlook_gpx &cache,
                               const native::rect &area,
                               bool preserve_items = true);
} // namespace linux::openlook
