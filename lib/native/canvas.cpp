//
// Implements the backend-neutral part of the paintable child surface.
// Content bounds, scrollbar policy, clamping, chrome geometry, themed
// scrollbar drawing, and pointer normalization are shared by every
// backend; only the drawing resource itself is created per backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include <native.h>
#include <native/canvas.h>

namespace
{
    using native::canvas_scroll_position;

    constexpr std::int32_t int32_min =
        std::numeric_limits<std::int32_t>::min();
    constexpr std::int32_t int32_max =
        std::numeric_limits<std::int32_t>::max();

    //
    // Return origin + delta clamped to the signed content range.
    //
    // Notes:
    //      Content coordinates are 32-bit, so every intermediate sum
    //      fits in a 64-bit accumulator and the clamp is the only
    //      thing this has to get right.
    //
    std::int32_t saturating_advance(std::int32_t origin,
                                    std::uint32_t delta) {
        const std::int64_t sum = static_cast<std::int64_t>(origin) +
                                 static_cast<std::int64_t>(delta);
        return sum > int32_max ? int32_max
                               : static_cast<std::int32_t>(sum);
    }

    // Return value + step clamped to the signed range on both ends.
    std::int32_t saturating_step(std::int32_t value, int step) {
        const std::int64_t sum = static_cast<std::int64_t>(value) +
                                 static_cast<std::int64_t>(step);
        if (sum < int32_min)
            return int32_min;
        if (sum > int32_max)
            return int32_max;
        return static_cast<std::int32_t>(sum);
    }

    // Return the distance a viewport can travel across content.
    std::uint32_t scroll_range(std::uint32_t span, int page) {
        const std::uint32_t viewport =
            static_cast<std::uint32_t>(std::max(0, page));
        return span > viewport ? span - viewport : 0;
    }

    //
    // Clamp one axis to its valid leading-edge interval.
    //
    // Notes:
    //      The interval is [origin, max(origin, origin + span - page)].
    //      Empty content has no valid interval at all and reports
    //      position zero, matching the portable contract.
    //
    std::int32_t clamp_axis(std::int32_t origin,
                            std::uint32_t span,
                            int page,
                            std::int32_t value) {
        if (span == 0)
            return 0;
        const std::int32_t last =
            saturating_advance(origin, scroll_range(span, page));
        if (value < origin)
            return origin;
        if (value > last)
            return last;
        return value;
    }

    // Return the offset of a position inside its scrollable range.
    std::uint32_t offset_in_range(std::int32_t origin,
                                  std::int32_t value) {
        if (value <= origin)
            return 0;
        return static_cast<std::uint32_t>(
            static_cast<std::int64_t>(value) -
            static_cast<std::int64_t>(origin));
    }

    // Reduce a rectangle's right and bottom edges by reserved chrome.
    native::rect reserve_edges(const native::rect &bounds,
                               int right,
                               int bottom) {
        const int width =
            std::max(0, static_cast<int>(bounds.d.w) - right);
        const int height =
            std::max(0, static_cast<int>(bounds.d.h) - bottom);
        return native::rect(bounds.p.x,
                            bounds.p.y,
                            static_cast<native::dim>(width),
                            static_cast<native::dim>(height));
    }
} // namespace

namespace native
{
    canvas::canvas(coord x, coord y, dim width, dim height)
        : custom_control(x, y, width, height) {}

    canvas::canvas(const point &position, const size &dimensions)
        : custom_control(position, dimensions) {}

    canvas::canvas(const rect &bounds)
        : custom_control(bounds) {}

    canvas::~canvas() {
        destroy();
    }

    canvas &canvas::set_content_bounds(canvas_content_bounds bounds) {
        _content = bounds;
        apply_scroll(_scroll);
        invalidate();
        return *this;
    }

    canvas_content_bounds canvas::get_content_bounds() const {
        return _content;
    }

    canvas &canvas::set_scroll_position(canvas_scroll_position position) {
        // A programmatic setter never emits the user-action signal.
        if (apply_scroll(position))
            invalidate();
        return *this;
    }

    canvas_scroll_position canvas::get_scroll_position() const {
        return _scroll;
    }

    canvas &canvas::set_horizontal_scrollbar_policy(
        scrollbar_policy policy) {
        if (_horizontal_policy == policy)
            return *this;
        _horizontal_policy = policy;
        apply_scroll(_scroll);
        relayout_children();
        invalidate();
        return *this;
    }

    scrollbar_policy canvas::get_horizontal_scrollbar_policy() const {
        return _horizontal_policy;
    }

    canvas &canvas::set_vertical_scrollbar_policy(
        scrollbar_policy policy) {
        if (_vertical_policy == policy)
            return *this;
        _vertical_policy = policy;
        apply_scroll(_scroll);
        relayout_children();
        invalidate();
        return *this;
    }

    scrollbar_policy canvas::get_vertical_scrollbar_policy() const {
        return _vertical_policy;
    }

    bool canvas::get_horizontal_scrollbar_visible() const {
        return resolve_geometry().horizontal_visible;
    }

    bool canvas::get_vertical_scrollbar_visible() const {
        return resolve_geometry().vertical_visible;
    }

    rect canvas::get_chrome_bounds() const {
        const scroll_geometry geometry = resolve_geometry();
        return reserve_edges(
            rect(0, 0, _bounds.d.w, _bounds.d.h),
            geometry.vertical_visible ? _scrollbar_extent : 0,
            geometry.horizontal_visible ? _scrollbar_extent : 0);
    }

    //
    // Resolve both scrollbar decisions and the viewport together.
    //
    // Notes:
    //      Showing one scrollbar shrinks the other axis, which can
    //      make that axis overflow in turn. Visibility therefore only
    //      ever grows inside this pass, so the loop reaches a stable
    //      answer and cannot oscillate.
    //
    canvas::scroll_geometry canvas::resolve_geometry() const {
        scroll_geometry geometry;
        geometry.horizontal_visible =
            _horizontal_policy == scrollbar_policy::always;
        geometry.vertical_visible =
            _vertical_policy == scrollbar_policy::always;

        const rect bounds(0, 0, _bounds.d.w, _bounds.d.h);
        for (int pass = 0; pass < 3; ++pass) {
            const rect chrome = reserve_edges(
                bounds,
                geometry.vertical_visible ? _scrollbar_extent : 0,
                geometry.horizontal_visible ? _scrollbar_extent : 0);
            geometry.viewport = reserve_non_client(chrome);

            const bool horizontal =
                geometry.horizontal_visible ||
                (_horizontal_policy == scrollbar_policy::automatic &&
                 _content.width > geometry.viewport.d.w);
            const bool vertical =
                geometry.vertical_visible ||
                (_vertical_policy == scrollbar_policy::automatic &&
                 _content.height > geometry.viewport.d.h);
            if (horizontal == geometry.horizontal_visible &&
                vertical == geometry.vertical_visible)
                break;
            geometry.horizontal_visible = horizontal;
            geometry.vertical_visible = vertical;
        }

        const int width = static_cast<int>(bounds.d.w);
        const int height = static_cast<int>(bounds.d.h);
        const int track_width = std::max(
            0, width - (geometry.vertical_visible ? _scrollbar_extent : 0));
        const int track_height = std::max(
            0,
            height - (geometry.horizontal_visible ? _scrollbar_extent : 0));

        if (geometry.horizontal_visible) {
            geometry.horizontal_track =
                rect(0,
                     static_cast<coord>(height - _scrollbar_extent),
                     static_cast<dim>(track_width),
                     static_cast<dim>(
                         std::min(height, _scrollbar_extent)));
        }
        if (geometry.vertical_visible) {
            geometry.vertical_track =
                rect(static_cast<coord>(width - _scrollbar_extent),
                     0,
                     static_cast<dim>(
                         std::min(width, _scrollbar_extent)),
                     static_cast<dim>(track_height));
        }
        if (geometry.horizontal_visible && geometry.vertical_visible) {
            geometry.corner =
                rect(static_cast<coord>(width - _scrollbar_extent),
                     static_cast<coord>(height - _scrollbar_extent),
                     static_cast<dim>(
                         std::min(width, _scrollbar_extent)),
                     static_cast<dim>(
                         std::min(height, _scrollbar_extent)));
        }
        return geometry;
    }

    rect canvas::thumb_bounds(const scroll_geometry &geometry,
                              scrollbar_orientation orientation) const {
        const bool horizontal =
            orientation == scrollbar_orientation::horizontal;
        const rect track = horizontal ? geometry.horizontal_track
                                      : geometry.vertical_track;
        const int length = horizontal ? static_cast<int>(track.d.w)
                                      : static_cast<int>(track.d.h);
        if (length <= 0)
            return track;

        const std::uint32_t span =
            horizontal ? _content.width : _content.height;
        const int page = horizontal
                             ? static_cast<int>(geometry.viewport.d.w)
                             : static_cast<int>(geometry.viewport.d.h);
        const std::uint32_t range = scroll_range(span, page);
        if (range == 0)
            return track;

        const double visible =
            static_cast<double>(std::max(0, page)) /
            static_cast<double>(span);
        int thumb = static_cast<int>(
            std::lround(static_cast<double>(length) * visible));
        thumb = std::clamp(thumb,
                           std::min(length, _scrollbar_min_thumb),
                           length);

        const int travel = length - thumb;
        const std::int32_t origin =
            horizontal ? _content.x : _content.y;
        const std::int32_t value = horizontal ? _scroll.x : _scroll.y;
        const std::uint32_t offset = offset_in_range(origin, value);
        int start = 0;
        if (travel > 0 && offset >= range) {
            start = travel;
        } else if (travel > 0) {
            start = static_cast<int>(
                std::lround(static_cast<double>(travel) *
                            (static_cast<double>(offset) /
                             static_cast<double>(range))));
            start = std::clamp(start, 0, travel);
        }

        if (horizontal)
            return rect(static_cast<coord>(track.p.x + start),
                        track.p.y,
                        static_cast<dim>(thumb),
                        track.d.h);
        return rect(track.p.x,
                    static_cast<coord>(track.p.y + start),
                    track.d.w,
                    static_cast<dim>(thumb));
    }

    std::int32_t canvas::position_from_track(
        const scroll_geometry &geometry,
        scrollbar_orientation orientation,
        int offset) const {
        const bool horizontal =
            orientation == scrollbar_orientation::horizontal;
        const rect track = horizontal ? geometry.horizontal_track
                                      : geometry.vertical_track;
        const rect thumb = thumb_bounds(geometry, orientation);
        const int length = horizontal ? static_cast<int>(track.d.w)
                                      : static_cast<int>(track.d.h);
        const int extent = horizontal ? static_cast<int>(thumb.d.w)
                                      : static_cast<int>(thumb.d.h);
        const std::int32_t origin =
            horizontal ? _content.x : _content.y;
        const std::uint32_t span =
            horizontal ? _content.width : _content.height;
        const int page = horizontal
                             ? static_cast<int>(geometry.viewport.d.w)
                             : static_cast<int>(geometry.viewport.d.h);
        const std::uint32_t range = scroll_range(span, page);
        const int travel = std::max(0, length - extent);

        // Both endpoints stay exactly reachable however coarse the
        // pixel track is compared with the content range.
        if (travel <= 0 || offset <= 0)
            return origin;
        if (offset >= travel)
            return saturating_advance(origin, range);

        // A 32-bit range converts to double exactly, so the only
        // approximation here is the pixel track's own resolution.
        const double fraction = static_cast<double>(offset) /
                                static_cast<double>(travel);
        const auto moved = static_cast<std::uint32_t>(std::llround(
            fraction * static_cast<double>(range)));
        return saturating_advance(origin, std::min(moved, range));
    }

    canvas::hit_part canvas::hit_test(const scroll_geometry &geometry,
                                      const point &position) const {
        if (geometry.vertical_visible &&
            geometry.vertical_track.contains(position)) {
            return thumb_bounds(geometry, scrollbar_orientation::vertical)
                           .contains(position)
                       ? hit_part::vertical_thumb
                       : hit_part::vertical_track;
        }
        if (geometry.horizontal_visible &&
            geometry.horizontal_track.contains(position)) {
            return thumb_bounds(geometry,
                                scrollbar_orientation::horizontal)
                           .contains(position)
                       ? hit_part::horizontal_thumb
                       : hit_part::horizontal_track;
        }
        return hit_part::none;
    }

    bool canvas::apply_scroll(canvas_scroll_position position) {
        const scroll_geometry geometry = resolve_geometry();
        const canvas_scroll_position clamped{
            clamp_axis(_content.x,
                       _content.width,
                       static_cast<int>(geometry.viewport.d.w),
                       position.x),
            clamp_axis(_content.y,
                       _content.height,
                       static_cast<int>(geometry.viewport.d.h),
                       position.y)};
        if (clamped.x == _scroll.x && clamped.y == _scroll.y)
            return false;
        _scroll = clamped;
        return true;
    }

    void canvas::on_native_scroll(canvas_scroll_position position) {
        if (!apply_scroll(position))
            return;
        invalidate();
        on_scroll.emit(_scroll);
    }

    void canvas::on_bounds_changed() {
        // A new viewport can shorten the valid interval on either axis.
        if (apply_scroll(_scroll))
            invalidate();
    }

    void canvas::synchronize_theme_metrics() {
        custom_control::synchronize_theme_metrics();
        _scrollbar_extent = std::max(
            1, _theme_metrics.scrollbar_extent);
        _scrollbar_min_thumb = std::max(
            1, _theme_metrics.scrollbar_min_thumb);
    }

    void canvas::draw_scrollbar(gpx &graphics,
                                const rect &track,
                                const rect &thumb,
                                scrollbar_orientation orientation,
                                const theme::state &element_state) {
        auto appearance = theme::create(graphics);
        appearance->draw_scrollbar_part(
            track, orientation, scrollbar_part::track, element_state);
        appearance->draw_scrollbar_part(
            thumb, orientation, scrollbar_part::thumb, element_state);
    }

    void canvas::on_native_paint(wnd_paint_event event) {
        const scroll_geometry geometry = resolve_geometry();

        // The subscriber owns every client pixel, so nothing is drawn
        // under or over it. Clipping keeps it out of the rulers and
        // the scrollbar tracks even when it ignores the invalid rect.
        {
            const auto client = event.g.save_state();
            const rect invalid = event.r.intersect(geometry.viewport);
            event.g.set_clip(invalid);
            wnd_paint_event client_event(invalid, event.g);
            on_wnd_paint.emit(client_event);
        }

        draw_non_client(event.g);

        const auto chrome = event.g.save_state();
        event.g.set_clip(rect(0, 0, _bounds.d.w, _bounds.d.h));
        if (geometry.horizontal_visible) {
            theme::state state;
            state.disabled =
                scroll_range(_content.width,
                             static_cast<int>(geometry.viewport.d.w)) == 0;
            state.hot = _hot == hit_part::horizontal_thumb;
            state.pressed = _pressed == hit_part::horizontal_thumb;
            draw_scrollbar(event.g,
                           geometry.horizontal_track,
                           thumb_bounds(geometry,
                                        scrollbar_orientation::horizontal),
                           scrollbar_orientation::horizontal,
                           state);
        }
        if (geometry.vertical_visible) {
            theme::state state;
            state.disabled =
                scroll_range(_content.height,
                             static_cast<int>(geometry.viewport.d.h)) == 0;
            state.hot = _hot == hit_part::vertical_thumb;
            state.pressed = _pressed == hit_part::vertical_thumb;
            draw_scrollbar(event.g,
                           geometry.vertical_track,
                           thumb_bounds(geometry,
                                        scrollbar_orientation::vertical),
                           scrollbar_orientation::vertical,
                           state);
        }
        if (geometry.corner.d.w && geometry.corner.d.h) {
            auto appearance = theme::create(event.g);
            appearance->draw_surface(
                geometry.corner, surface_kind::panel, theme::state{});
        }
    }

    void canvas::on_native_mouse_move(const point &position) {
        const scroll_geometry geometry = resolve_geometry();

        if (_pressed == hit_part::horizontal_thumb ||
            _pressed == hit_part::vertical_thumb) {
            const bool horizontal =
                _pressed == hit_part::horizontal_thumb;
            const scrollbar_orientation orientation =
                horizontal ? scrollbar_orientation::horizontal
                           : scrollbar_orientation::vertical;
            const rect track = horizontal ? geometry.horizontal_track
                                          : geometry.vertical_track;
            const int pointer = horizontal ? position.x : position.y;
            const int origin = horizontal ? track.p.x : track.p.y;
            const int offset = pointer - origin - _drag_offset;
            canvas_scroll_position moved = _scroll;
            const std::int32_t value =
                position_from_track(geometry, orientation, offset);
            if (horizontal)
                moved.x = value;
            else
                moved.y = value;
            on_native_scroll(moved);
            return;
        }

        const hit_part hot = hit_test(geometry, position);
        if (hot != _hot) {
            _hot = hot;
            invalidate();
        }
        if (hot != hit_part::none)
            return;

        wnd::on_native_mouse_move(position);
    }

    void canvas::on_native_mouse_click(mouse_event event) {
        const scroll_geometry geometry = resolve_geometry();

        if (event.action == mouse_action::release) {
            const bool was_chrome = _pressed != hit_part::none;
            _pressed = hit_part::none;
            if (was_chrome) {
                invalidate();
                return;
            }
            wnd::on_native_mouse_click(event);
            return;
        }

        const hit_part part = hit_test(geometry, event.position);
        if (part == hit_part::none) {
            wnd::on_native_mouse_click(event);
            return;
        }

        if (part == hit_part::horizontal_thumb ||
            part == hit_part::vertical_thumb) {
            const bool horizontal = part == hit_part::horizontal_thumb;
            const rect thumb = thumb_bounds(
                geometry,
                horizontal ? scrollbar_orientation::horizontal
                           : scrollbar_orientation::vertical);
            _drag_offset = horizontal ? event.position.x - thumb.p.x
                                      : event.position.y - thumb.p.y;
            _pressed = part;
            invalidate();
            return;
        }

        // A press in the empty track pages toward the pointer.
        const bool horizontal = part == hit_part::horizontal_track;
        const rect thumb = thumb_bounds(
            geometry,
            horizontal ? scrollbar_orientation::horizontal
                       : scrollbar_orientation::vertical);
        const int pointer = horizontal ? event.position.x
                                       : event.position.y;
        const int leading = horizontal ? thumb.p.x : thumb.p.y;
        const int page = horizontal
                             ? static_cast<int>(geometry.viewport.d.w)
                             : static_cast<int>(geometry.viewport.d.h);
        const bool forward =
            pointer >= leading + (horizontal
                                      ? static_cast<int>(thumb.d.w)
                                      : static_cast<int>(thumb.d.h));
        const int step = forward ? std::max(1, page) : -std::max(1, page);
        canvas_scroll_position moved = _scroll;
        if (horizontal)
            moved.x = saturating_step(moved.x, step);
        else
            moved.y = saturating_step(moved.y, step);
        _pressed = part;
        on_native_scroll(moved);
    }

    void canvas::on_native_mouse_wheel(mouse_wheel_event event) {
        const scroll_geometry geometry = resolve_geometry();
        const bool horizontal =
            event.direction == wheel_direction::horizontal;
        const std::uint32_t range = scroll_range(
            horizontal ? _content.width : _content.height,
            horizontal ? static_cast<int>(geometry.viewport.d.w)
                       : static_cast<int>(geometry.viewport.d.h));

        // The normalized event still reaches the subscriber exactly
        // once, whether or not the canvas could move.
        if (range != 0) {
            canvas_scroll_position moved = _scroll;
            if (horizontal)
                moved.x = saturating_step(moved.x, -event.delta);
            else
                moved.y = saturating_step(moved.y, -event.delta);
            on_native_scroll(moved);
        }

        wnd::on_native_mouse_wheel(event);
    }
} // namespace native
