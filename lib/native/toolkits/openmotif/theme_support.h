//
// Declares private OpenMotif theme resource and drawing helpers shared
// by the semantic theme implementation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <Xm/Xm.h>

#include <native.h>

namespace linux::openmotif
{
    struct motif_gpx;

    // Identifies a drawable Motif window graphics target.
    struct theme_target
    {
        Widget widget = nullptr;
        motif_gpx *cache = nullptr;
    };

    // Resolve the native window target represented by a graphics context.
    theme_target theme_target_from(native::gpx &graphics);

    // Find a widget whose resource database applies to the target.
    Widget theme_reference_widget(native::gpx &graphics);

    // Convert one widget-colormap pixel to portable RGBA.
    native::rgba theme_pixel_color(Widget widget, Pixel pixel);

    // Build a portable palette from live Motif control resources.
    native::theme::palette theme_palette(Widget reference,
                                         Widget button,
                                         Widget list);

    // Query an owned X font structure for the Motif control font.
    XFontStruct *theme_control_font();

    // Create a clipped graphics context for an Xme primitive.
    GC theme_gc(const theme_target &target,
                Pixel color,
                const native::rect &clip);
} // namespace linux::openmotif
