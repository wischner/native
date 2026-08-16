//
// Keeps XView frames reachable using XRandR work areas and EWMH
// decoration dimensions when the active window manager supplies them.
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

#include <native.h>
#include <native/screen.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <xview/xview.h>

#include "globals.h"

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

    std::int64_t overlap_area(
        const native::rect &bounds,
        const native::point &position,
        const native::size &dimensions) {
        const int left = std::max<int>(bounds.p.x, position.x);
        const int top = std::max<int>(bounds.p.y, position.y);
        const int right = std::min<int>(
            bounds.x2(), position.x + dimensions.w);
        const int bottom = std::min<int>(
            bounds.y2(), position.y + dimensions.h);
        if (right <= left || bottom <= top)
            return 0;
        return static_cast<std::int64_t>(right - left) *
               (bottom - top);
    }

    std::int64_t distance_to(
        const native::rect &bounds, int x, int y) {
        const int right = bounds.x2() - 1;
        const int bottom = bounds.y2() - 1;
        const std::int64_t dx =
            x < bounds.p.x ? bounds.p.x - x
                           : (x > right ? x - right : 0);
        const std::int64_t dy =
            y < bounds.p.y ? bounds.p.y - y
                           : (y > bottom ? y - bottom : 0);
        return dx * dx + dy * dy;
    }

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
            const std::int64_t overlap = overlap_area(
                candidate.bounds(), position, dimensions);
            const std::int64_t distance = distance_to(
                candidate.bounds(), center_x, center_y);
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

    Window frame_xid(Frame frame) {
        return frame
                   ? static_cast<Window>(xv_get(frame, XV_XID))
                   : None;
    }

    frame_extents get_frame_extents(Frame frame) {
        frame_extents extents;
        Display *display = linux::openlook::cached_display;
        const Window window = frame_xid(frame);
        if (!display || window == None)
            return extents;

        const Atom property_atom = XInternAtom(
            display, "_NET_FRAME_EXTENTS", True);
        if (property_atom == None)
            return extents;

        Atom actual_type = None;
        int actual_format = 0;
        unsigned long item_count = 0;
        unsigned long bytes_after = 0;
        unsigned char *property = nullptr;
        const int result = XGetWindowProperty(
            display,
            window,
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

    native::rect root_work_area() {
        Display *display = linux::openlook::cached_display;
        if (!display)
            return native::rect(0, 0, 640, 480);
        const int screen = DefaultScreen(display);
        return native::rect(
            0,
            0,
            static_cast<native::dim>(DisplayWidth(display, screen)),
            static_cast<native::dim>(DisplayHeight(display, screen)));
    }
} // namespace

namespace linux::openlook
{
    native::point frame_position(Frame frame) {
        Display *display = cached_display;
        Window window = frame_xid(frame);
        if (!display || window == None)
            return native::point();

        Window root = None;
        Window parent = None;
        Window *children = nullptr;
        unsigned int child_count = 0;
        while (XQueryTree(display,
                          window,
                          &root,
                          &parent,
                          &children,
                          &child_count)) {
            if (children) {
                XFree(children);
                children = nullptr;
            }
            if (parent == None || parent == root)
                break;
            window = parent;
        }

        int x = 0;
        int y = 0;
        Window child = None;
        if (root != None) {
            XTranslateCoordinates(display,
                                  window,
                                  root,
                                  0,
                                  0,
                                  &x,
                                  &y,
                                  &child);
        }
        return native::point(
            static_cast<native::coord>(x),
            static_cast<native::coord>(y));
    }

    void request_frame_extents(Frame frame) {
        Display *display = cached_display;
        const Window window = frame_xid(frame);
        if (!display || window == None)
            return;

        XEvent request = {};
        request.xclient.type = ClientMessage;
        request.xclient.display = display;
        request.xclient.window = window;
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

    native::point constrain_frame_position(
        Frame frame,
        const native::point &preferred,
        const native::size &dimensions) {
        native::rect work_area = root_work_area();
        try {
            const auto &screens = native::screen::detect();
            const native::screen *selected = select_screen(
                screens, preferred, dimensions);
            if (selected)
                work_area = selected->work_area();
        } catch (const std::exception &) {
            // Root geometry remains a safe placement fallback.
        }

        const frame_extents extents = get_frame_extents(frame);
        const int minimum_x = work_area.p.x + extents.left;
        const int maximum_x = work_area.x2() - dimensions.w -
                              extents.right;
        const int minimum_y = work_area.p.y + extents.top;
        const int maximum_y = work_area.y2() - dimensions.h -
                              extents.bottom;
        const int x = maximum_x < minimum_x
                          ? minimum_x
                          : std::clamp<int>(
                                preferred.x, minimum_x, maximum_x);
        const int y = maximum_y < minimum_y
                          ? minimum_y
                          : std::clamp<int>(
                                preferred.y, minimum_y, maximum_y);
        return native::point(
            static_cast<native::coord>(x),
            static_cast<native::coord>(y));
    }
} // namespace linux::openlook
