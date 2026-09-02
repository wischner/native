//
// Declares the private platform adapter for a native status-bar peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>

#include <native/geometry.h>

namespace native
{
    class status_bar;

    namespace detail
    {
        class status_bar_peer
        {
        public:
            virtual ~status_bar_peer() = default;

            // Synchronize a native peer and return true when it owns paint.
            virtual bool update(status_bar &bar, const rect &bounds) = 0;
        };

        std::unique_ptr<status_bar_peer> create_status_bar_peer();
    } // namespace detail
} // namespace native
