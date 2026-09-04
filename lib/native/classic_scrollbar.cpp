//
// Implements overflow-safe classic scrollbar geometry, thumb dragging, and
// the shared themed arrow-button, trough, and thumb presentation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "classic_scrollbar.h"

#include <algorithm>

namespace native::detail
{
    namespace
    {
        // Return the main-axis length of a scrollbar rectangle.
        int axis_length(const rect &bounds,
                        scrollbar_orientation orientation) {
            return orientation == scrollbar_orientation::vertical
                       ? static_cast<int>(bounds.d.h)
                       : static_cast<int>(bounds.d.w);
        }

        // Return the cross-axis thickness of a scrollbar rectangle.
        int cross_extent(const rect &bounds,
                         scrollbar_orientation orientation) {
            return orientation == scrollbar_orientation::vertical
                       ? static_cast<int>(bounds.d.w)
                       : static_cast<int>(bounds.d.h);
        }
    } // namespace

    classic_scrollbar_geometry make_classic_scrollbar(
        const rect &bounds,
        scrollbar_orientation orientation,
        std::uint64_t total,
        std::uint64_t page,
        std::uint64_t value,
        int minimum_thumb) {
        classic_scrollbar_geometry result;
        result.bounds = bounds;
        const int length = axis_length(bounds, orientation);
        const int arrow = std::min(
            std::max(0, cross_extent(bounds, orientation)),
            std::max(0, length / 2));
        if (orientation == scrollbar_orientation::vertical) {
            result.decrement = rect(
                bounds.p.x, bounds.p.y, bounds.d.w,
                static_cast<dim>(arrow));
            result.increment = rect(
                bounds.p.x,
                static_cast<coord>(bounds.y2() - arrow),
                bounds.d.w,
                static_cast<dim>(arrow));
            result.trough = rect(
                bounds.p.x,
                static_cast<coord>(bounds.p.y + arrow),
                bounds.d.w,
                static_cast<dim>(std::max(0, length - arrow * 2)));
        } else {
            result.decrement = rect(
                bounds.p.x, bounds.p.y,
                static_cast<dim>(arrow), bounds.d.h);
            result.increment = rect(
                static_cast<coord>(bounds.x2() - arrow),
                bounds.p.y,
                static_cast<dim>(arrow), bounds.d.h);
            result.trough = rect(
                static_cast<coord>(bounds.p.x + arrow),
                bounds.p.y,
                static_cast<dim>(std::max(0, length - arrow * 2)),
                bounds.d.h);
        }

        total = std::max<std::uint64_t>(1, total);
        page = std::clamp<std::uint64_t>(page, 1, total);
        const int trough_length = axis_length(result.trough, orientation);
        const int proportional = static_cast<int>(
            static_cast<long double>(trough_length) * page / total);
        const int thumb_length = std::clamp(
            std::max(minimum_thumb, proportional), 0, trough_length);
        const std::uint64_t maximum = total - page;
        value = std::min(value, maximum);
        const int travel = std::max(0, trough_length - thumb_length);
        const int position = maximum == 0
            ? 0
            : static_cast<int>(
                  static_cast<long double>(travel) * value / maximum);
        if (orientation == scrollbar_orientation::vertical) {
            result.thumb = rect(
                result.trough.p.x,
                static_cast<coord>(result.trough.p.y + position),
                result.trough.d.w,
                static_cast<dim>(thumb_length));
        } else {
            result.thumb = rect(
                static_cast<coord>(result.trough.p.x + position),
                result.trough.p.y,
                static_cast<dim>(thumb_length),
                result.trough.d.h);
        }
        return result;
    }

    std::uint64_t classic_scrollbar_drag_value(
        const classic_scrollbar_geometry &scrollbar,
        scrollbar_orientation orientation,
        int pointer_coordinate,
        int grab_offset,
        std::uint64_t total,
        std::uint64_t page) {
        total = std::max<std::uint64_t>(1, total);
        page = std::clamp<std::uint64_t>(page, 1, total);
        const std::uint64_t maximum = total - page;
        const int trough_start =
            orientation == scrollbar_orientation::vertical
                ? scrollbar.trough.y1()
                : scrollbar.trough.x1();
        const int trough_length = axis_length(
            scrollbar.trough, orientation);
        const int thumb_length = axis_length(
            scrollbar.thumb, orientation);
        const int travel = std::max(0, trough_length - thumb_length);
        if (travel == 0 || maximum == 0)
            return 0;
        const int position = std::clamp(
            pointer_coordinate - grab_offset - trough_start,
            0,
            travel);
        return static_cast<std::uint64_t>(
            static_cast<long double>(maximum) * position / travel);
    }

    void draw_classic_scrollbar(
        theme &appearance,
        scrollbar_orientation orientation,
        const rect &bounds,
        const rect &thumb,
        const theme::state &state) {
        const int length = axis_length(bounds, orientation);
        const int arrow = std::min(
            std::max(0, cross_extent(bounds, orientation)),
            std::max(0, length / 2));
        const rect decrement = orientation == scrollbar_orientation::vertical
            ? rect(bounds.p.x, bounds.p.y, bounds.d.w,
                   static_cast<dim>(arrow))
            : rect(bounds.p.x, bounds.p.y, static_cast<dim>(arrow),
                   bounds.d.h);
        const rect increment = orientation == scrollbar_orientation::vertical
            ? rect(bounds.p.x,
                   static_cast<coord>(bounds.y2() - arrow),
                   bounds.d.w,
                   static_cast<dim>(arrow))
            : rect(static_cast<coord>(bounds.x2() - arrow),
                   bounds.p.y,
                   static_cast<dim>(arrow),
                   bounds.d.h);
        const rect trough = orientation == scrollbar_orientation::vertical
            ? rect(bounds.p.x,
                   static_cast<coord>(bounds.p.y + arrow),
                   bounds.d.w,
                   static_cast<dim>(std::max(0, length - arrow * 2)))
            : rect(static_cast<coord>(bounds.p.x + arrow),
                   bounds.p.y,
                   static_cast<dim>(std::max(0, length - arrow * 2)),
                   bounds.d.h);
        appearance.draw_scrollbar_part(
            trough, orientation, scrollbar_part::track, state);
        appearance.draw_scrollbar_part(
            decrement, orientation, scrollbar_part::decrement, state);
        appearance.draw_scrollbar_part(
            increment, orientation, scrollbar_part::increment, state);
        appearance.draw_scrollbar_part(
            thumb, orientation, scrollbar_part::thumb, state);
    }
} // namespace native::detail
