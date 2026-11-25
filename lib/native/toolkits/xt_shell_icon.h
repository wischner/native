//
// Declares window icons for Xt shells used by Athena and Motif.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Intrinsic.h>

namespace native
{
    class img;
    class wnd;
} // namespace native

namespace native::detail
{
    //
    // Apply a window icon to an Xt top-level shell.
    //
    // Parameters:
    //      owner       - Window whose peer retains the icon pixmaps.
    //      shell       - WMShell-derived top-level shell widget.
    //      icon        - Source image, or null for the manager default.
    //
    // Notes:
    //      ICCCM pixmaps use the iconPixmap and iconMask resources, so
    //      Xt applies them at realization. A realized shell also
    //      receives EWMH images at once. Replaced pixmaps are freed.
    //
    void apply_xt_shell_icon(wnd &owner, Widget shell,
                             const img *icon);

    // Publish EWMH images after a shell has just been realized.
    void publish_xt_shell_icon(Widget shell, const img *icon);

    // Free retained pixmaps while the display connection is still open.
    void release_xt_shell_icon(wnd &owner);
} // namespace native::detail
