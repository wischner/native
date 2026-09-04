//
// Declares an empty portable child container for hosting and laying out
// other Native windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "theme.h"
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

        // Paint the themed container background before child content.
        void on_native_paint(wnd_paint_event event) override;

    protected:
        // Create the backend child-container resource.
        void create_native() override;

        // Destroy the backend child-container resource.
        void destroy_native() override;

        // Show an already-created panel.
        void show_native() override;

        // Draw the complete structural-container background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);
    };
} // namespace native
