//
// Declares the X11 desktop-scale boundary. Native geometry is in
// logical pixels; Xt widgets, Xlib drawables, pointer events and
// screen geometry are device pixels. The factor is the desktop's
// Xft.dpi / 96, the value X clients that scale themselves follow, so
// a compositor that already upscales X windows (and publishes 96)
// never has it doubled.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <native/geometry.h>

namespace linux::x11
{
    //
    // Return the factor from logical pixels to device pixels.
    //
    // Returns:
    //      Xft.dpi / 96 rounded to a quarter step, between 1 and 4.
    //      The value is read once from the first open display; 1 is
    //      returned (and not cached) before a display exists.
    //
    double coordinate_scale();

    // Convert a logical length or position to device pixels.
    int to_device(int logical);

    // Convert a device length or position to logical pixels.
    int to_logical(int device);

    // Convert a device pointer position to the logical pixel it lies in.
    int pointer_to_logical(int device);

    // Convert a device pointer position to a logical point.
    native::point pointer_to_logical(int x, int y);

    // Convert a device size to logical pixels.
    native::size size_to_logical(int width, int height);

    // Convert a device rectangle to logical pixels.
    native::rect rect_to_logical(const native::rect &device);

    //
    // Convert a logical rectangle to the device pixels it covers.
    //
    // Notes:
    //      Each edge is converted separately, so adjacent logical
    //      rectangles tile without gaps or overlap at any factor.
    //
    native::rect rect_to_device(const native::rect &logical);

    // Convert a logical extent to a non-zero device widget dimension.
    Dimension device_dimension(int logical);

    //
    // Scale the Athena spacing resources for a newly opened display.
    //
    // Parameters:
    //      shell - the application shell returned by Xt initialization
    //
    // Notes:
    //      Called once, before any library widget exists. The device
    //      control font, margins and spacings go into the display
    //      database at loose precedence, so explicitly configured
    //      resources keep their values. Border and rule widths are
    //      strokes and stay one device pixel. The unscaled control
    //      font name is remembered for the logical font role.
    //
    void scale_resources(Widget shell);

    // Return the unscaled Athena control font name, empty if unknown.
    const std::string &logical_control_font();

    //
    // Return the configured "*font" resource, or nullptr if none.
    //
    // Notes:
    //      Xt installs its database as the display database, so after
    //      scale_resources XGetDefault reports the device font. This is
    //      the value it reported before.
    //
    const char *logical_default_font(Display *display);

    //
    // Load the device-resolution counterpart of a logical core font.
    //
    // Returns:
    //      A newly loaded font the caller owns, or 0 when the factor
    //      is 1 or no bitmap face is installed.
    //
    // Notes:
    //      Chooses the installed, unscaled bitmap nearest the logical
    //      pixel size times the factor: the same family first, then
    //      Helvetica/Lucida (proportional) or Lucida Typewriter,
    //      Terminus and Fixed (character cell).
    //
    Font load_device_font(Display *display, const XFontStruct *logical);

    //
    // Create a bitmap from XBM data with each bit scaled to device size.
    //
    // Returns:
    //      A depth-1 pixmap the caller owns, or None on failure.
    //
    Pixmap create_device_bitmap(Display *display,
                                Drawable drawable,
                                const char *bits,
                                unsigned int width,
                                unsigned int height);
} // namespace linux::x11
