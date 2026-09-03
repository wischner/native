//
// Declares an empty portable child container for hosting and laying out
// other Native windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "wnd.h"

namespace native
{
    // Provides a concrete structural host for borrowed child windows.
    class panel : public wnd
    {
    public:
        // Construct a panel from scalar bounds.
        panel(coord x = 0,
              coord y = 0,
              dim width = 320,
              dim height = 240);

        // Construct a panel from a position and dimensions.
        panel(const point &position, const size &dimensions);

        // Construct a panel from complete bounds.
        explicit panel(const rect &bounds);

        // Destroy the panel and its native resource if it exists.
        ~panel() override;

        // Create the backend child-container resource.
        void create() const override;

        // Destroy the backend child-container resource.
        void destroy() const override;

        // Show an already-created panel.
        void show() const override;
    };
} // namespace native
