//
// Declares the private X11 clipboard service used when SDL2 lacks image
// clipboard representations on its active Linux video platform.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "../../clipboard_backend.h"

namespace linux::sdl2::x11_clipboard
{
    // Return whether the X11 platform clipboard is available.
    bool available();

    // Copy the current X11 CLIPBOARD selection.
    native::detail::clipboard_payload read();

    // Own and publish a complete X11 CLIPBOARD selection.
    void write(const native::detail::clipboard_payload &payload);

    // Serve delayed selection requests without a second event loop.
    void service();

    // Release the private X11 connection during SDL shutdown.
    void shutdown();
} // namespace linux::sdl2::x11_clipboard
