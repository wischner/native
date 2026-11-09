//
// Declares configurable horizontal and vertical non-client rulers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

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

    // Selects how a ruler marker is drawn.
    enum class ruler_marker_shape
    {
        // Points away from the owner's content: a left or right indent.
        down_triangle,

        // Points toward the content: a first-line indent.
        up_triangle,

        // A small block: a page margin edge.
        block,

        // A short bar: a tab stop.
        tab
    };

    // One position an application shows, and optionally lets users move.
    struct ruler_marker
    {
        int id = 0;
        double value = 0.0;
        ruler_marker_shape shape = ruler_marker_shape::down_triangle;
        bool movable = true;
    };

    // Reports a marker being moved by the pointer.
    struct ruler_marker_move
    {
        int id = 0;
        double value = 0.0;

        // True on release, when the gesture is complete.
        bool finished = false;
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

        // Return whether a rule is drawn along the strip's trailing edge.
        bool get_edge_visible() const;

        // Show or hide the trailing edge rule.
        ruler &set_edge_visible(bool visible);

        // Return the latest tracked logical value, when available.
        std::optional<double> get_tracked_value() const;

        // Highlight the span between two values, such as the text area
        // between page margins, in the content color.
        ruler &set_active_range(double first, double last);

        // Remove the highlighted span.
        ruler &clear_active_range();

        // Replace the markers. Ids identify them in on_marker_move.
        ruler &set_markers(std::vector<ruler_marker> markers);

        // Return the markers.
        const std::vector<ruler_marker> &get_markers() const;

        // Round dragged values to multiples of an interval; 0 disables it.
        ruler &set_marker_snap(double interval);

        // Emits the current ruler value after pointer tracking changes.
        signal<double> on_tracking;

        // Emits while a marker is dragged and once more on release.
        signal<ruler_marker_move> on_marker_move;

    protected:
        // Paint the complete ruler into its current edge strip.
        void draw(gpx &graphics, const rect &bounds) override;

        // Convert host pointer motion into a logical ruler value.
        void track_pointer(const point &position) override;
        // Route presses/releases to movable markers.
        bool pointer_click(const mouse_event &event) override;
        // Clear an interrupted drag without emitting a completed move.
        void pointer_cancel() override;
        // Report whether this strip is dragging a marker.
        bool get_pointer_held() const override;

        // Draw one marker at an axis position.
        virtual void draw_marker(gpx &graphics,
                                 const rect &bounds,
                                 int axis_position,
                                 const ruler_marker &marker,
                                 const theme::palette &colors);

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

        // Draw the optional bottom or right ruler edge.
        virtual void draw_edge(gpx &graphics,
                               const rect &bounds,
                               const theme::palette &colors);

    private:
        double _origin = 0.0;
        double _units_per_pixel = 1.0;
        double _minor_tick = 10.0;
        double _major_tick = 50.0;
        bool _track_mouse = false;
        bool _edge_visible = false;
        std::optional<double> _tracked_value;
        std::optional<int> _tracked_axis;
        std::optional<std::pair<double, double>> _active_range;
        std::vector<ruler_marker> _markers;
        double _marker_snap = 0.0;
        int _dragged_marker = -1;

        // Return the axis pixel of a value, along the strip.
        int axis_of(double value) const;

        // Return the strip-relative axis of an owner-local point.
        int axis_at(const point &position) const;

        // Return the value at an axis pixel, snapped.
        double value_at(int axis) const;

        // Convert an edge into its horizontal or vertical orientation.
        static ruler_orientation orientation_for(window_edge edge);
    };
} // namespace native
