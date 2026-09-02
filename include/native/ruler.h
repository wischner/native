//
// Declares configurable horizontal and vertical non-client rulers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <optional>
#include <string>

#include "non_client.h"
#include "signal.h"
#include "theme.h"

namespace native
{
    enum class ruler_orientation
    {
        horizontal,
        vertical
    };

    class ruler : public non_client
    {
    public:
        // Construct a top or left ruler from its logical orientation.
        ruler(wnd &owner,
              ruler_orientation orientation,
              int extent = 24);

        // Construct a ruler attached to any host edge.
        ruler(wnd &owner, window_edge edge, int extent = 24);

        // Return the orientation implied by the current edge.
        ruler_orientation get_orientation() const;

        // Return the value at the ruler's leading edge.
        double get_origin() const;

        // Set the finite value at the ruler's leading edge.
        ruler &set_origin(double origin);

        // Return the logical units represented by one pixel.
        double get_units_per_pixel() const;

        // Set a positive, finite logical scale.
        ruler &set_units_per_pixel(double units_per_pixel);

        // Return the interval between minor ticks.
        double get_minor_tick() const;

        // Set a positive, finite minor-tick interval.
        ruler &set_minor_tick(double interval);

        // Return the interval between labeled major ticks.
        double get_major_tick() const;

        // Set a positive, finite major-tick interval.
        ruler &set_major_tick(double interval);

        // Return whether host pointer motion updates the tracker.
        bool get_track_mouse() const;

        // Enable or disable host pointer tracking.
        ruler &set_track_mouse(bool enabled);

        // Return the latest tracked logical value, when available.
        std::optional<double> get_tracked_value() const;

        // Emits the current ruler value after pointer tracking changes.
        signal<double> on_tracking;

    protected:
        // Paint the complete ruler into its current edge strip.
        void draw(gpx &graphics, const rect &bounds) override;

        // Convert host pointer motion into a logical ruler value.
        void track_pointer(const point &position) override;

        // Draw the ruler's native-themed strip background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one minor or major tick at an axis coordinate.
        virtual void draw_tick(gpx &graphics,
                               const rect &bounds,
                               int axis_position,
                               bool major,
                               const theme::palette &colors);

        // Draw one major-tick label.
        virtual void draw_label(gpx &graphics,
                                const point &position,
                                const std::string &text,
                                const theme::palette &colors);

        // Draw the optional pointer-tracking marker.
        virtual void draw_tracker(gpx &graphics,
                                  const rect &bounds,
                                  int axis_position,
                                  const theme::palette &colors);

    private:
        double _origin = 0.0;
        double _units_per_pixel = 1.0;
        double _minor_tick = 10.0;
        double _major_tick = 50.0;
        bool _track_mouse = false;
        std::optional<double> _tracked_value;
        std::optional<int> _tracked_axis;

        // Convert an edge into its horizontal or vertical orientation.
        static ruler_orientation orientation_for(window_edge edge);
    };
} // namespace native
