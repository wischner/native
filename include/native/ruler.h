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
        ruler(wnd &owner,
              ruler_orientation orientation,
              int extent = 24);

        ruler(wnd &owner, window_edge edge, int extent = 24);

        ruler_orientation get_orientation() const;

        double get_origin() const;
        ruler &set_origin(double origin);

        double get_units_per_pixel() const;
        ruler &set_units_per_pixel(double units_per_pixel);

        double get_minor_tick() const;
        ruler &set_minor_tick(double interval);

        double get_major_tick() const;
        ruler &set_major_tick(double interval);

        bool get_track_mouse() const;
        ruler &set_track_mouse(bool enabled);

        std::optional<double> get_tracked_value() const;

        // Emits the current ruler value after pointer tracking changes.
        signal<double> on_tracking;

    protected:
        void draw(gpx &graphics, const rect &bounds) override;
        void track_pointer(const point &position) override;

        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        virtual void draw_tick(gpx &graphics,
                               const rect &bounds,
                               int axis_position,
                               bool major,
                               const theme::palette &colors);

        virtual void draw_label(gpx &graphics,
                                const point &position,
                                const std::string &text,
                                const theme::palette &colors);

        virtual void draw_tracker(gpx &graphics,
                                  const rect &bounds,
                                  int axis_position,
                                  const theme::palette &colors);

    private:
        ruler_orientation _orientation;
        double _origin = 0.0;
        double _units_per_pixel = 1.0;
        double _minor_tick = 10.0;
        double _major_tick = 50.0;
        bool _track_mouse = false;
        std::optional<double> _tracked_value;
        std::optional<int> _tracked_axis;

        static ruler_orientation orientation_for(window_edge edge);
    };
} // namespace native
