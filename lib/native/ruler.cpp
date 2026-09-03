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
        auto appearance = theme::create(graphics);
        const theme::state state{};
        const theme::palette colors = appearance->native_palette();
        graphics.set_font(font_t::stock(font_role::small));
        draw_background(graphics, *appearance, bounds, state);

        const ruler_orientation orientation = get_orientation();
        const int length = orientation == ruler_orientation::horizontal
            ? bounds.w() : bounds.h();
        const double final_value = _origin + length * _units_per_pixel;
        const double first_value =
            std::ceil(_origin / _minor_tick) * _minor_tick;
        const int maximum_ticks = length * 4 + 8;
        int count = 0;
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
            if (major) {
                const point label =
                    orientation == ruler_orientation::horizontal
                    ? point(static_cast<coord>(bounds.x1() + axis + 2),
                            static_cast<coord>(bounds.y1() + 2))
                    : point(static_cast<coord>(bounds.x1() + 2),
                            static_cast<coord>(bounds.y1() + axis + 2));
                draw_label(graphics, label, format_value(value), colors);
            }
        }

        if (_edge_visible)
            draw_edge(graphics, bounds, colors);

        if (_track_mouse && _tracked_axis)
            draw_tracker(graphics, bounds, *_tracked_axis, colors);
    }

    void ruler::track_pointer(const point &position) {
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
        appearance.draw_surface(bounds, surface_kind::header, state);
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
        graphics.set_ink(colors.focus);
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
