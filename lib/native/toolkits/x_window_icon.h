//
// Declares window-icon publication shared by X11-based toolkits.
// Toolkits own their ICCCM hint path; EWMH images are common.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Xlib.h>
#include <native/graphics.h>

namespace native::detail
{
    // Server pixmaps describing one ICCCM icon and its shape.
    struct x_icon_pixmaps
    {
        Pixmap image = None;
        Pixmap mask = None;
        int depth = 0;
    };

    //
    // Choose a square ICCCM icon edge for one screen.
    //
    // Parameters:
    //      display     - Open display connection.
    //      screen      - Screen whose root carries WM_ICON_SIZE.
    //      preferred   - The toolkit's conventional icon edge.
    //
    // Returns:
    //      The preferred edge when no manager advertises sizes;
    //      otherwise the acceptable edge nearest to it, honoring the
    //      advertised minimum, maximum and increments.
    //
    dim x_icon_edge(Display *display, int screen, dim preferred = 48);

    //
    // Create ICCCM image and mask pixmaps for an icon.
    //
    // Parameters:
    //      display     - Open display connection.
    //      screen      - Screen whose default visual receives the icon.
    //      icon        - Source image; it is fitted into a square.
    //      edge        - Square pixmap edge in pixels.
    //
    // Returns:
    //      Pixmaps owned by the caller. TrueColor screens receive a
    //      default-depth color image; other screens receive a dithered
    //      depth-1 bitmap. The depth-1 mask keeps alpha of 128 or more.
    //
    x_icon_pixmaps create_x_icon_pixmaps(Display *display, int screen,
                                         const img &icon, dim edge);

    // Release both pixmaps, if present, and reset them to None.
    void free_x_icon_pixmaps(Display *display,
                             x_icon_pixmaps &pixmaps);

    //
    // Publish EWMH _NET_WM_ICON images on an existing window.
    //
    // Parameters:
    //      display     - Open display connection.
    //      window      - Top-level client window.
    //      icon        - Source image, or null to delete the property.
    //
    // Notes:
    //      Square-fitted straight-alpha ARGB copies are published at
    //      16, 32, 48, 64 and 128 pixels.
    //
    void set_x_net_wm_icon(Display *display, Window window,
                           const img *icon);

    // Remove ICCCM icon pixmap and mask hints from an existing window.
    void clear_x_icon_hints(Display *display, Window window);
} // namespace native::detail
