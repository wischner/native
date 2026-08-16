//
// Keeps X11 top-level shells reachable within an XRandR monitor work
// area using EWMH frame dimensions when the window manager supplies
// them.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "window_position.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <vector>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <native/screen.h>

namespace
{
    constexpr int fallback_title_height = 48;

    struct frame_extents
    {
        int left = 0;
        int right = 0;
        int top = fallback_title_height;
        int bottom = 0;
    };

    // Calculate client overlap without narrowing through public coords.
    std::int64_t overlap_area(const native::rect &bounds,
                              const native::point &position,
                              const native::size &dimensions) {
        const int left = std::max<int>(bounds.p.x, position.x);
        const int top = std::max<int>(bounds.p.y, position.y);
        const int right = std::min<int>(
            bounds.p.x + bounds.d.w, position.x + dimensions.w);
        const int bottom = std::min<int>(
            bounds.p.y + bounds.d.h, position.y + dimensions.h);
        if (right <= left || bottom <= top)
            return 0;
        return static_cast<std::int64_t>(right - left) *
               (bottom - top);
    }

    // Measure the squared distance from a point to screen bounds.
    std::int64_t distance_to(const native::rect &bounds,
                             int x,
                             int y) {
        const int right = bounds.p.x + bounds.d.w - 1;
        const int bottom = bounds.p.y + bounds.d.h - 1;
        const std::int64_t dx =
            x < bounds.p.x ? bounds.p.x - x
                           : (x > right ? x - right : 0);
        const std::int64_t dy =
            y < bounds.p.y ? bounds.p.y - y
                           : (y > bottom ? y - bottom : 0);
        return dx * dx + dy * dy;
    }

    // Choose the screen containing most of the client, or the closest
    // one when the preferred rectangle is outside the virtual desktop.
    const native::screen *select_screen(
        const std::vector<native::screen> &screens,
        const native::point &position,
        const native::size &dimensions) {
        const native::screen *best = nullptr;
        std::int64_t best_overlap = 0;
        std::int64_t best_distance =
            std::numeric_limits<std::int64_t>::max();
        const int center_x = position.x + dimensions.w / 2;
        const int center_y = position.y + dimensions.h / 2;

        for (const native::screen &candidate : screens) {
            const std::int64_t overlap =
                overlap_area(candidate.bounds(),
                             position,
                             dimensions);
            const std::int64_t distance =
                distance_to(candidate.bounds(), center_x, center_y);
            if (!best || overlap > best_overlap ||
                (overlap == best_overlap &&
                 distance < best_distance)) {
                best = &candidate;
                best_overlap = overlap;
                best_distance = distance;
            }
        }
        return best;
    }

    // Read the EWMH left, right, top, and bottom shell frame values.
    frame_extents get_frame_extents(Widget shell) {
        frame_extents extents;
        if (!shell || !XtIsRealized(shell))
            return extents;

        Display *display = XtDisplay(shell);
        Atom property_atom =
            XInternAtom(display, "_NET_FRAME_EXTENTS", True);
        if (property_atom == None)
            return extents;

        Atom actual_type = None;
        int actual_format = 0;
        unsigned long item_count = 0;
        unsigned long bytes_after = 0;
        unsigned char *property = nullptr;
        const int result = XGetWindowProperty(display,
                                              XtWindow(shell),
                                              property_atom,
                                              0,
                                              4,
                                              False,
                                              XA_CARDINAL,
                                              &actual_type,
                                              &actual_format,
                                              &item_count,
                                              &bytes_after,
                                              &property);
        if (result == Success && actual_type == XA_CARDINAL &&
            actual_format == 32 && item_count >= 4) {
            auto *values =
                reinterpret_cast<unsigned long *>(property);
            extents.left = static_cast<int>(values[0]);
            extents.right = static_cast<int>(values[1]);
            extents.top = static_cast<int>(values[2]);
            extents.bottom = static_cast<int>(values[3]);
        }
        if (property)
            XFree(property);
        return extents;
    }

    // Fall back to the X screen when XRandR detection is unavailable.
    native::rect root_work_area(Widget shell) {
        Screen *x_screen = XtScreen(shell);
        return native::rect(
            0,
            0,
            static_cast<native::dim>(WidthOfScreen(x_screen)),
            static_cast<native::dim>(HeightOfScreen(x_screen)));
    }
} // namespace

namespace linux::x11
{
    void request_frame_extents(Widget shell) {
        if (!shell || !XtIsRealized(shell))
            return;

        Display *display = XtDisplay(shell);
        XEvent request = {};
        request.xclient.type = ClientMessage;
        request.xclient.display = display;
        request.xclient.window = XtWindow(shell);
        request.xclient.message_type = XInternAtom(
            display, "_NET_REQUEST_FRAME_EXTENTS", False);
        request.xclient.format = 32;
        XSendEvent(display,
                   DefaultRootWindow(display),
                   False,
                   SubstructureRedirectMask |
                       SubstructureNotifyMask,
                   &request);
    }

    native::point constrain_shell_position(
        Widget shell,
        const native::point &preferred,
        const native::size &dimensions) {
        native::rect work_area = root_work_area(shell);
        try {
            const auto &screens = native::screen::detect();
            const native::screen *selected =
                select_screen(screens, preferred, dimensions);
            if (selected)
                work_area = selected->work_area();
        } catch (const std::exception &) {
            // Root geometry still keeps the title reachable when
            // XRandR or work-area discovery is unavailable.
        }

        const frame_extents frame = get_frame_extents(shell);
        const int minimum_x = work_area.p.x + frame.left;
        const int maximum_x = work_area.p.x + work_area.d.w -
                              dimensions.w - frame.right;
        const int minimum_y = work_area.p.y + frame.top;
        const int maximum_y = work_area.p.y + work_area.d.h -
                              dimensions.h - frame.bottom;
        const int x = maximum_x < minimum_x
                          ? minimum_x
                          : std::clamp<int>(preferred.x,
                                            minimum_x,
                                            maximum_x);
        const int y = maximum_y < minimum_y
                          ? minimum_y
                          : std::clamp<int>(preferred.y,
                                            minimum_y,
                                            maximum_y);
        return native::point(static_cast<native::coord>(x),
                             static_cast<native::coord>(y));
    }
} // namespace linux::x11
