//
// Declares edge-attached non-client elements shared by rulers and status bars.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "geometry.h"

namespace native
{
    class gpx;
    class wnd;

    // Selects the host-window edge occupied by a non-client element.
    enum class window_edge
    {
        top,
        right,
        bottom,
        left
    };

    // Reserves and paints one strip outside a window's laid-out client area.
    class non_client
    {
    public:
        non_client(wnd &owner, window_edge edge, int extent);
        virtual ~non_client();

        non_client(const non_client &) = delete;
        non_client &operator=(const non_client &) = delete;

        // Return the host window, or null after that host is destroyed.
        wnd *get_owner() const;

        // Return or change the occupied edge.
        window_edge get_edge() const;
        non_client &set_edge(window_edge edge);

        // Return or change the non-negative edge thickness in pixels.
        int get_extent() const;
        non_client &set_extent(int extent);

        // Return or change whether the strip is visible and reserves space.
        bool get_visible() const;
        non_client &set_visible(bool visible);

        // Return the current host-relative strip bounds.
        rect get_bounds() const;

    protected:
        // Paint the strip after the host's client paint handlers.
        virtual void draw(gpx &graphics, const rect &bounds) = 0;

        // Observe host-relative pointer motion for optional tracking.
        virtual void track_pointer(const point &position);

        // Repaint this strip without changing its geometry.
        void invalidate() const;

    private:
        friend class wnd;

        wnd *_owner;
        window_edge _edge;
        int _extent;
        bool _visible;
    };
} // namespace native
