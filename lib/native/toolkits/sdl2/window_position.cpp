//
// Keeps SDL2 top-level windows reachable on the selected display,
// including when a client is taller than the usable work area.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "window_position.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
    constexpr int fallback_title_height = 48;

    struct frame_extents
    {
        int top = fallback_title_height;
        int left = 0;
        int bottom = 0;
        int right = 0;
    };

    // Return the area shared by two integer screen rectangles.
    std::int64_t overlap_area(const SDL_Rect &first,
                              const SDL_Rect &second) {
        const int left = std::max(first.x, second.x);
        const int top = std::max(first.y, second.y);
        const int right = std::min(first.x + first.w,
                                   second.x + second.w);
        const int bottom = std::min(first.y + first.h,
                                    second.y + second.h);
        if (right <= left || bottom <= top)
            return 0;
        return static_cast<std::int64_t>(right - left) *
               (bottom - top);
    }

    // Measure the squared distance from a point to a display rectangle.
    std::int64_t distance_to(const SDL_Rect &bounds,
                             int x,
                             int y) {
        const int right = bounds.x + bounds.w - 1;
        const int bottom = bounds.y + bounds.h - 1;
        const std::int64_t dx =
            x < bounds.x ? bounds.x - x
                         : (x > right ? x - right : 0);
        const std::int64_t dy =
            y < bounds.y ? bounds.y - y
                         : (y > bottom ? y - bottom : 0);
        return dx * dx + dy * dy;
    }

    // Select the display with the greatest client overlap, or the
    // nearest display when the requested client is entirely off-screen.
    int display_for(const SDL_Rect &requested) {
        const int count = SDL_GetNumVideoDisplays();
        int best_display = -1;
        std::int64_t best_overlap = 0;
        std::int64_t best_distance =
            std::numeric_limits<std::int64_t>::max();
        const int center_x = requested.x + requested.w / 2;
        const int center_y = requested.y + requested.h / 2;

        for (int index = 0; index < count; ++index) {
            SDL_Rect bounds = {};
            if (SDL_GetDisplayBounds(index, &bounds) != 0)
                continue;

            const std::int64_t overlap =
                overlap_area(requested, bounds);
            const std::int64_t distance =
                distance_to(bounds, center_x, center_y);
            if (best_display < 0 || overlap > best_overlap ||
                (overlap == best_overlap &&
                 distance < best_distance)) {
                best_display = index;
                best_overlap = overlap;
                best_distance = distance;
            }
        }
        return best_display;
    }

    // Query decorations after mapping and retain conservative values
    // when the window manager has not published them yet.
    frame_extents get_frame_extents(SDL_Window *window) {
        frame_extents extents;
        if (!window)
            return extents;

#if SDL_VERSION_ATLEAST(2, 0, 5)
        int top = 0;
        int left = 0;
        int bottom = 0;
        int right = 0;
        if (SDL_GetWindowBordersSize(window,
                                     &top,
                                     &left,
                                     &bottom,
                                     &right) == 0 &&
            (top != 0 || left != 0 || bottom != 0 || right != 0 ||
             (SDL_GetWindowFlags(window) &
              SDL_WINDOW_BORDERLESS) != 0)) {
            extents.top = top;
            extents.left = left;
            extents.bottom = bottom;
            extents.right = right;
        }
#endif
        return extents;
    }
} // namespace

namespace linux::sdl2
{
    native::point constrain_window_position(
        SDL_Window *window,
        const native::point &preferred,
        const native::size &dimensions) {
        SDL_Rect requested = {
            preferred.x,
            preferred.y,
            static_cast<int>(dimensions.w),
            static_cast<int>(dimensions.h)
        };
        const int display = display_for(requested);

        SDL_Rect usable = {};
        if (display < 0 ||
            SDL_GetDisplayUsableBounds(display, &usable) != 0) {
            return preferred;
        }

        const frame_extents frame = get_frame_extents(window);
        const int minimum_x = usable.x + frame.left;
        const int maximum_x = usable.x + usable.w - requested.w -
                              frame.right;
        const int minimum_y = usable.y + frame.top;
        const int maximum_y = usable.y + usable.h - requested.h -
                              frame.bottom;
        const int x = maximum_x < minimum_x
                          ? minimum_x
                          : std::clamp(requested.x,
                                       minimum_x,
                                       maximum_x);
        const int y = maximum_y < minimum_y
                          ? minimum_y
                          : std::clamp(requested.y,
                                       minimum_y,
                                       maximum_y);
        return native::point(static_cast<native::coord>(x),
                             static_cast<native::coord>(y));
    }
} // namespace linux::sdl2
