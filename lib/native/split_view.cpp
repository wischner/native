//
// Implements portable split-view state, layout, and fallback interaction.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/split_view.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <native/graphics.h>

namespace native
{
    split_view::split_view(wnd &first,
                           wnd &second,
                           split_orientation orientation,
                           coord x,
                           coord y,
                           dim width,
                           dim height)
        : wnd(x, y, width, height)
        , _first(&first)
        , _second(&second)
        , _orientation(orientation) {
        if (&first == &second)
            throw std::invalid_argument(
                "split view panes must be different windows");
        if (first.get_created() || second.get_created())
            throw std::invalid_argument(
                "split view panes must be uncreated when attached");
        first.set_parent(this);
        second.set_parent(this);
        set_cursor(orientation == split_orientation::horizontal
                       ? mouse_cursor::resize_horizontal
                       : mouse_cursor::resize_vertical);
        on_wnd_paint.connect([this](wnd_paint_event event) {
            draw(event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return handle_click(event);
        });
        on_mouse_move.connect([this](point position) {
            return handle_move(position);
        });
    }

    split_view::split_view(wnd &first,
                           wnd &second,
                           split_orientation orientation,
                           const rect &bounds)
        : split_view(first, second, orientation,
                     bounds.p.x, bounds.p.y,
                     bounds.d.w, bounds.d.h) {}

    split_view::~split_view() {
        destroy();
        if (_first && _first->get_parent() == this)
            _first->set_parent(nullptr);
        if (_second && _second->get_parent() == this)
            _second->set_parent(nullptr);
    }

    wnd &split_view::get_first() const { return *_first; }
    wnd &split_view::get_second() const { return *_second; }

    split_orientation split_view::get_orientation() const {
        return _orientation;
    }

    split_view &split_view::set_orientation(
        split_orientation orientation) {
        if (_orientation == orientation)
            return *this;
        _orientation = orientation;
        set_cursor(orientation == split_orientation::horizontal
                       ? mouse_cursor::resize_horizontal
                       : mouse_cursor::resize_vertical);
        if (_created)
            apply_orientation();
        refresh_contents();
        invalidate();
        return *this;
    }

    float split_view::get_ratio() const { return _ratio; }

    split_view &split_view::set_ratio(float ratio) {
        if (!std::isfinite(ratio))
            throw std::invalid_argument("split ratio must be finite");
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        if (std::abs(_ratio-ratio) < 0.0001f)
            return *this;
        _ratio = ratio;
        if (_created)
            apply_ratio();
        refresh_contents();
        invalidate();
        return *this;
    }

    dim split_view::get_first_minimum() const { return _first_minimum; }
    dim split_view::get_second_minimum() const { return _second_minimum; }

    split_view &split_view::set_minimums(dim first, dim second) {
        _first_minimum = first;
        _second_minimum = second;
        if (_created)
            apply_minimums();
        refresh_contents();
        invalidate();
        return *this;
    }

    dim split_view::get_splitter_size() const { return _splitter_size; }

    split_view &split_view::set_splitter_size(dim size) {
        _splitter_size = size;
        if (_created)
            apply_splitter_size();
        refresh_contents();
        invalidate();
        return *this;
    }

    int split_view::resolved_first_extent() const {
        const int total = _orientation == split_orientation::horizontal
            ? static_cast<int>(_bounds.d.w)
            : static_cast<int>(_bounds.d.h);
        const int available = std::max(
            0, total-static_cast<int>(_splitter_size));
        int first = static_cast<int>(std::lround(available*_ratio));
        const int first_min = std::min(
            available, static_cast<int>(_first_minimum));
        const int second_min = std::min(
            available-first_min,
            static_cast<int>(_second_minimum));
        first = std::clamp(first, first_min, available-second_min);
        return first;
    }

    rect split_view::get_first_bounds() const {
        const int first = resolved_first_extent();
        return _orientation == split_orientation::horizontal
            ? rect(0, 0, static_cast<dim>(first), _bounds.d.h)
            : rect(0, 0, _bounds.d.w, static_cast<dim>(first));
    }

    rect split_view::get_splitter_bounds() const {
        const int first = resolved_first_extent();
        return _orientation == split_orientation::horizontal
            ? rect(static_cast<coord>(first), 0,
                   _splitter_size, _bounds.d.h)
            : rect(0, static_cast<coord>(first),
                   _bounds.d.w, _splitter_size);
    }

    rect split_view::get_second_bounds() const {
        const int first = resolved_first_extent();
        const int splitter = static_cast<int>(_splitter_size);
        if (_orientation == split_orientation::horizontal) {
            const int width = std::max(
                0, static_cast<int>(_bounds.d.w)-first-splitter);
            return rect(static_cast<coord>(first+splitter), 0,
                        static_cast<dim>(width), _bounds.d.h);
        }
        const int height = std::max(
            0, static_cast<int>(_bounds.d.h)-first-splitter);
        return rect(0, static_cast<coord>(first+splitter),
                    _bounds.d.w, static_cast<dim>(height));
    }

    void split_view::refresh_contents() {
        rect first = get_first_bounds();
        rect second = get_second_bounds();
        if (_content_hosts_are_panes) {
            first.p = point(0, 0);
            second.p = point(0, 0);
        }
        _first->set_bounds(first);
        _second->set_bounds(second);
        if (_created) {
            if (!_first->get_created())
                _first->create();
            if (!_second->get_created())
                _second->create();
            _first->show();
            _second->show();
        }
    }

    void split_view::on_native_ratio(float ratio) {
        const float previous = _ratio;
        set_ratio(ratio);
        if (std::abs(previous-_ratio) >= 0.0001f)
            on_ratio_change.emit(_ratio);
    }

    void split_view::on_bounds_changed() {
        refresh_contents();
        if (_created)
            apply_ratio();
        invalidate();
    }

    float split_view::ratio_from_position(const point &position) const {
        const int total = _orientation == split_orientation::horizontal
            ? static_cast<int>(_bounds.d.w)
            : static_cast<int>(_bounds.d.h);
        const int available = std::max(
            1, total-static_cast<int>(_splitter_size));
        const int offset = _orientation == split_orientation::horizontal
            ? static_cast<int>(position.x)
            : static_cast<int>(position.y);
        return std::clamp(
            static_cast<float>(offset)/available, 0.0f, 1.0f);
    }

    bool split_view::handle_click(const mouse_event &event) {
        if (event.button != mouse_button::left)
            return false;
        if (event.action == mouse_action::press) {
            _dragging = get_splitter_bounds().contains(event.position);
            return _dragging;
        }
        const bool handled = _dragging;
        if (_dragging)
            on_native_ratio(ratio_from_position(event.position));
        _dragging = false;
        return handled;
    }

    bool split_view::handle_move(const point &position) {
        if (!_dragging)
            return false;
        on_native_ratio(ratio_from_position(position));
        return true;
    }

    void split_view::draw(gpx &graphics) {
        auto appearance = theme::create(graphics);
        const rect bounds = get_splitter_bounds();
        const theme::state state{false, _dragging};
        draw_splitter_background(
            graphics, *appearance, bounds, state);
        draw_splitter_grip(
            graphics, *appearance, bounds, state);
    }

    void split_view::draw_splitter_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::panel, state);
    }

    void split_view::draw_splitter_grip(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        const rgba ink = appearance.get_separator_color();
        graphics.set_ink(ink);
        if (_orientation == split_orientation::horizontal) {
            const coord x = static_cast<coord>(
                bounds.p.x + static_cast<int>(bounds.d.w) / 2);
            const coord y = static_cast<coord>(
                bounds.p.y + static_cast<int>(bounds.d.h) / 2);
            graphics.draw_line(point(x, static_cast<coord>(y - 6)),
                               point(x, static_cast<coord>(y + 6)));
        } else {
            const coord x = static_cast<coord>(
                bounds.p.x + static_cast<int>(bounds.d.w) / 2);
            const coord y = static_cast<coord>(
                bounds.p.y + static_cast<int>(bounds.d.h) / 2);
            graphics.draw_line(point(static_cast<coord>(x - 6), y),
                               point(static_cast<coord>(x + 6), y));
        }
    }
} // namespace native
