//
// Implements themed edge rulers with optional pointer tracking.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <native/graphics.h>
#include <native/ruler.h>

#include "rotated_text.h"

namespace
{
    std::string format_value(double value) {
        std::ostringstream stream;
        if (std::abs(value - std::round(value)) < 0.000001)
            stream << static_cast<long long>(std::llround(value));
        else
            stream << std::fixed << std::setprecision(2) << value;
        return stream.str();
    }
}

namespace native
{
    ruler::ruler(wnd &owner,
                 ruler_orientation orientation,
                 int extent)
        : non_client(owner,
                     orientation == ruler_orientation::horizontal
                        ? window_edge::top : window_edge::left,
                     extent) {}

    ruler::ruler(wnd &owner, window_edge edge, int extent)
        : non_client(owner, edge, extent) {}

    ruler_orientation ruler::orientation_for(window_edge edge) {
        return edge == window_edge::top || edge == window_edge::bottom
            ? ruler_orientation::horizontal
            : ruler_orientation::vertical;
    }

    ruler_orientation ruler::get_orientation() const {
        return orientation_for(get_edge());
    }

    double ruler::get_origin() const { return _origin; }

    ruler &ruler::set_origin(double origin) {
        if (!std::isfinite(origin))
            throw std::invalid_argument("A ruler origin must be finite.");
        _origin = origin;
        invalidate();
        return *this;
    }

    double ruler::get_units_per_pixel() const { return _units_per_pixel; }

    ruler &ruler::set_units_per_pixel(double units_per_pixel) {
        if (!std::isfinite(units_per_pixel) || units_per_pixel <= 0.0)
            throw std::invalid_argument(
                "Ruler units per pixel must be positive and finite.");
        _units_per_pixel = units_per_pixel;
        invalidate();
        return *this;
    }

    double ruler::get_minor_tick() const { return _minor_tick; }

    ruler &ruler::set_minor_tick(double interval) {
        if (!std::isfinite(interval) || interval <= 0.0)
            throw std::invalid_argument(
                "A ruler minor tick interval must be positive and finite.");
        _minor_tick = interval;
        invalidate();
        return *this;
    }

    double ruler::get_major_tick() const { return _major_tick; }

    ruler &ruler::set_major_tick(double interval) {
        if (!std::isfinite(interval) || interval <= 0.0)
            throw std::invalid_argument(
                "A ruler major tick interval must be positive and finite.");
        _major_tick = interval;
        invalidate();
        return *this;
    }

    bool ruler::get_track_mouse() const { return _track_mouse; }

    ruler &ruler::set_track_mouse(bool enabled) {
        _track_mouse = enabled;
        if (!enabled) {
            _tracked_value.reset();
            _tracked_axis.reset();
        }
        invalidate();
        return *this;
    }

    bool ruler::get_edge_visible() const { return _edge_visible; }

    ruler &ruler::set_edge_visible(bool visible) {
        if (_edge_visible == visible)
            return *this;
        _edge_visible = visible;
        invalidate();
        return *this;
    }

    std::optional<double> ruler::get_tracked_value() const {
        return _tracked_value;
    }

    void ruler::draw(gpx &graphics, const rect &bounds) {
        if (!bounds.w() || !bounds.h())
            return;
        const auto saved = graphics.save_state();
        graphics.set_clip(graphics.get_clip().intersect(bounds));
        auto appearance = theme::create(graphics);
        const theme::state state{};
        theme::palette colors = appearance->native_palette();
        colors.button_text = colors.menu_text;
        graphics.set_font(font_t::stock(font_role::small));
        draw_background(graphics, *appearance, bounds, state);

        const ruler_orientation orientation = get_orientation();
        if (_active_range) {
            const bool horizontal =
                orientation == ruler_orientation::horizontal;
            const int origin = horizontal ? bounds.x1() : bounds.y1();
            const int extent = horizontal ? bounds.w() : bounds.h();
            const int first = std::clamp(origin + axis_of(_active_range->first),
                                         origin, origin + extent);
            const int last = std::clamp(origin + axis_of(_active_range->second),
                                        origin, origin + extent);
            if (last > first) {
                graphics.set_ink(colors.content_bg);
                graphics.draw_rect(
                    horizontal
                        ? rect(static_cast<coord>(first), bounds.y1(),
                               static_cast<dim>(last - first), bounds.h())
                        : rect(bounds.x1(), static_cast<coord>(first),
                               bounds.w(), static_cast<dim>(last - first)),
                    true);
            }
        }

        const int length = orientation == ruler_orientation::horizontal
            ? bounds.w() : bounds.h();
        const double final_value = _origin + length * _units_per_pixel;
        const double first_value =
            std::ceil(_origin / _minor_tick) * _minor_tick;
        const int maximum_ticks = length * 4 + 8;
        int count = 0;
        int next_label_axis = 0;
        for (double value = first_value;
             value <= final_value + _minor_tick * 0.001 &&
                 count < maximum_ticks;
             value += _minor_tick, ++count) {
            const int axis = static_cast<int>(std::lround(
                (value - _origin) / _units_per_pixel));
            const double major_multiple = value / _major_tick;
            const bool major = std::abs(major_multiple -
                                        std::round(major_multiple)) < 0.00001;
            draw_tick(graphics, bounds, axis, major, colors);
            if (major && axis >= next_label_axis) {
                const auto text = format_value(value);
                const int label_width = graphics.measure_text(text).width;
                // Keep dense tick marks, but leave readable space between
                // labels in both ordinary and rotated orientations.
                next_label_axis = axis + label_width + 6;
                if (axis + label_width + 2 > length)
                    continue;
                const point label =
                    orientation == ruler_orientation::horizontal
                    ? point(static_cast<coord>(bounds.x1() + axis + 2),
                            static_cast<coord>(bounds.y1() + 2))
                    : point(static_cast<coord>(bounds.x1() + 2),
                            static_cast<coord>(bounds.y1() + axis + 2));
                draw_label(graphics, label, text, colors);
            }
        }

        if (_edge_visible)
            draw_edge(graphics, bounds, colors);

        for (const ruler_marker &marker : _markers)
            draw_marker(graphics, bounds, axis_of(marker.value), marker,
                        colors);

        if (_track_mouse && _tracked_axis)
            draw_tracker(graphics, bounds, *_tracked_axis, colors);
        graphics.set_pen(1).set_ink(colors.button_bg)
            .draw_border(bounds, static_cast<border_sides>(
                unsigned(border_sides::all) ^ unsigned(get_border_sides())));
    }

    int ruler::axis_of(double value) const {
        return static_cast<int>(
            std::lround((value - _origin) / _units_per_pixel));
    }

    int ruler::axis_at(const point &position) const {
        const rect bounds = get_bounds();
        return get_orientation() == ruler_orientation::horizontal
            ? static_cast<int>(position.x) - bounds.x1()
            : static_cast<int>(position.y) - bounds.y1();
    }

    double ruler::value_at(int axis) const {
        const double value = _origin + axis * _units_per_pixel;
        return _marker_snap > 0.0
            ? std::round(value / _marker_snap) * _marker_snap
            : value;
    }

    ruler &ruler::set_active_range(double first, double last) {
        if (!std::isfinite(first) || !std::isfinite(last))
            throw std::invalid_argument(
                "A ruler active range must be finite.");
        _active_range = std::make_pair(first, last);
        invalidate();
        return *this;
    }

    ruler &ruler::clear_active_range() {
        _active_range.reset();
        invalidate();
        return *this;
    }

    ruler &ruler::set_markers(std::vector<ruler_marker> markers) {
        for (const ruler_marker &marker : markers)
            if (!std::isfinite(marker.value))
                throw std::invalid_argument(
                    "A ruler marker value must be finite.");
        // An application echoing a drag back must not end the drag.
        if (_dragged_marker >= 0 &&
            std::none_of(markers.begin(), markers.end(),
                         [this](const ruler_marker &marker) {
                             return marker.id == _dragged_marker;
                         }))
            _dragged_marker = -1;
        _markers = std::move(markers);
        invalidate();
        return *this;
    }

    const std::vector<ruler_marker> &ruler::get_markers() const {
        return _markers;
    }

    ruler &ruler::set_marker_snap(double interval) {
        if (!std::isfinite(interval) || interval < 0.0)
            throw std::invalid_argument(
                "A ruler snap interval cannot be negative.");
        _marker_snap = interval;
        return *this;
    }

    bool ruler::get_pointer_held() const {
        return _dragged_marker >= 0;
    }

    bool ruler::pointer_click(const mouse_event &event) {
        if (event.button != mouse_button::left)
            return false;
        if (event.action == mouse_action::release) {
            if (_dragged_marker < 0)
                return false;
            const int id = _dragged_marker;
            _dragged_marker = -1;
            on_marker_move.emit(
                {id, value_at(axis_at(event.position)), true});
            return true;
        }
        const int axis = axis_at(event.position);
        const rect bounds = get_bounds();
        const bool horizontal =
            get_orientation() == ruler_orientation::horizontal;
        const int across = horizontal ? event.position.y - bounds.y1()
                                      : event.position.x - bounds.x1();
        const int cross = horizontal ? bounds.h() : bounds.w();
        // Markers can share a pixel, as a page margin and an indent of zero
        // do. Where the press lands across the strip tells them apart.
        const auto band = [](ruler_marker_shape shape) {
            return shape == ruler_marker_shape::up_triangle ? 0
                : shape == ruler_marker_shape::block        ? 1
                                                            : 2;
        };
        const int pressed_band = std::clamp(across * 3 / std::max(cross, 1),
                                            0, 2);
        constexpr int grab_distance = 5;
        int best = (grab_distance + 1) * 4;
        for (const ruler_marker &marker : _markers) {
            const int distance = std::abs(axis - axis_of(marker.value));
            if (!marker.movable || distance > grab_distance)
                continue;
            const int score =
                distance * 4 + std::abs(band(marker.shape) - pressed_band);
            if (score < best) {
                best = score;
                _dragged_marker = marker.id;
            }
        }
        return _dragged_marker >= 0;
    }

    void ruler::draw_marker(gpx &graphics,
                            const rect &bounds,
                            int axis_position,
                            const ruler_marker &marker,
                            const theme::palette &colors) {
        const bool horizontal =
            get_orientation() == ruler_orientation::horizontal;
        const int cross = horizontal ? bounds.h() : bounds.w();
        const auto place = [&](int along, int across) {
            return horizontal
                ? point(static_cast<coord>(bounds.x1() + axis_position + along),
                        static_cast<coord>(bounds.y1() + across))
                : point(static_cast<coord>(bounds.x1() + across),
                        static_cast<coord>(bounds.y1() + axis_position + along));
        };
        constexpr int half = 4;
        graphics.set_pen(1).set_ink(colors.button_text);
        switch (marker.shape) {
        case ruler_marker_shape::down_triangle:
            graphics.draw_polygon({place(-half, cross - half - 1),
                                   place(half, cross - half - 1),
                                   place(0, cross - 1)},
                                  true);
            break;
        case ruler_marker_shape::up_triangle:
            graphics.draw_polygon(
                {place(-half, half), place(half, half), place(0, 0)}, true);
            break;
        case ruler_marker_shape::block:
            graphics.draw_polygon({place(-1, 0), place(1, 0),
                                   place(1, cross - 1), place(-1, cross - 1)},
                                  true);
            break;
        case ruler_marker_shape::tab:
            graphics.draw_line(place(0, cross - 7), place(0, cross - 2));
            graphics.draw_line(place(0, cross - 2), place(4, cross - 2));
            break;
        }
    }

    void ruler::track_pointer(const point &position) {
        if (_dragged_marker >= 0) {
            const double value = value_at(axis_at(position));
            for (ruler_marker &marker : _markers) {
                if (marker.id == _dragged_marker && marker.value != value) {
                    marker.value = value;
                    invalidate();
                    on_marker_move.emit({marker.id, value, false});
                    break;
                }
            }
            return;
        }
        if (!_track_mouse)
            return;
        const rect bounds = get_bounds();
        const ruler_orientation orientation = get_orientation();
        const int axis = orientation == ruler_orientation::horizontal
            ? static_cast<int>(position.x) - bounds.x1()
            : static_cast<int>(position.y) - bounds.y1();
        const int length = orientation == ruler_orientation::horizontal
            ? bounds.w() : bounds.h();
        if (axis < 0 || axis >= length)
            return;
        const double value = _origin + axis * _units_per_pixel;
        if (_tracked_axis && *_tracked_axis == axis)
            return;
        _tracked_axis = axis;
        _tracked_value = value;
        invalidate();
        on_tracking.emit(value);
    }

    void ruler::draw_background(gpx &,
                                theme &appearance,
                                const rect &bounds,
                                const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::ruler, state);
    }

    void ruler::draw_tick(gpx &graphics,
                          const rect &bounds,
                          int axis_position,
                          bool major,
                          const theme::palette &colors) {
        graphics.set_ink(colors.button_text);
        const int length = major ? 9 : 5;
        const window_edge edge = get_edge();
        if (get_orientation() == ruler_orientation::horizontal) {
            const coord x = static_cast<coord>(bounds.x1()+axis_position);
            const coord outer = edge == window_edge::top
                ? static_cast<coord>(bounds.y2()-1) : bounds.y1();
            const coord inner = static_cast<coord>(
                outer + (edge == window_edge::top ? -length : length));
            graphics.draw_line({x, outer}, {x, inner});
        } else {
            const coord y = static_cast<coord>(bounds.y1()+axis_position);
            const coord outer = edge == window_edge::left
                ? static_cast<coord>(bounds.x2()-1) : bounds.x1();
            const coord inner = static_cast<coord>(
                outer + (edge == window_edge::left ? -length : length));
            graphics.draw_line({outer, y}, {inner, y});
        }
    }

    void ruler::draw_label(gpx &graphics,
                           const point &position,
                           const std::string &text,
                           const theme::palette &colors) {
        graphics.set_ink(colors.button_text);
        if (get_orientation() == ruler_orientation::horizontal) {
            graphics.draw_text(text, position);
            return;
        }

        const text_metrics metrics = graphics.measure_text(text);
        const rect label_bounds(
            static_cast<coord>(position.x - 2),
            position.y,
            static_cast<dim>(std::max(1, metrics.height + 4)),
            static_cast<dim>(std::max(1, metrics.width)));
        detail::draw_rotated_text(
            graphics,
            text,
            label_bounds,
            get_edge() == window_edge::left);
    }

    void ruler::draw_tracker(gpx &graphics,
                             const rect &bounds,
                             int axis_position,
                             const theme::palette &colors) {
        graphics.set_ink(colors.button_text);
        if (get_orientation() == ruler_orientation::horizontal) {
            const coord x = static_cast<coord>(bounds.x1()+axis_position);
            graphics.draw_line({x, bounds.y1()},
                               {x, static_cast<coord>(bounds.y2()-1)});
        } else {
            const coord y = static_cast<coord>(bounds.y1()+axis_position);
            graphics.draw_line({bounds.x1(), y},
                               {static_cast<coord>(bounds.x2()-1), y});
        }
    }

    void ruler::draw_edge(gpx &graphics,
                          const rect &bounds,
                          const theme::palette &colors) {
        graphics.set_ink(colors.button_text);
        if (get_orientation() == ruler_orientation::horizontal) {
            const coord y = static_cast<coord>(bounds.y2() - 1);
            graphics.draw_line({bounds.x1(), y},
                               {static_cast<coord>(bounds.x2() - 1), y});
        } else {
            const coord x = static_cast<coord>(bounds.x2() - 1);
            graphics.draw_line({x, bounds.y1()},
                               {x, static_cast<coord>(bounds.y2() - 1)});
        }
    }
} // namespace native
