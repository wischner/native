//
// Declares optional native edge-toolbar controls hosted by the existing
// canvas peer. Backends without suitable controls retain themed painting.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/geometry.h>
#include <vector>
namespace native
{
    class toolbar;
    class canvas;
    namespace detail
    {
        // Query native ownership without creating controls during painting.
        bool toolbar_peer_active(const toolbar &owner, const canvas &surface);
        // Synchronize native controls and return whether they own tool painting.
        bool refresh_toolbar_peer(toolbar &owner, canvas &surface,
                                  const std::vector<rect> &bounds);
    }
}
