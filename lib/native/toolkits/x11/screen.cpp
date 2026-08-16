//
// Implements the X11 display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <native.h>
#include <native/screen.h>

#include "globals.h"

namespace native
{
    // Return the work area advertised by an EWMH window manager.
    static rect get_work_area_if_supported(Display *display,
                                           Window root) {
        Atom net_workarea = XInternAtom(display, "_NET_WORKAREA", True);
        if (net_workarea == None)
            return {};

        Atom actual_type = None;
        int actual_format = 0;
        unsigned long item_count = 0;
        unsigned long bytes_after = 0;
        unsigned char *property = nullptr;

        const int result = XGetWindowProperty(display,
                                              root,
                                              net_workarea,
                                              0,
                                              4,
                                              False,
                                              XA_CARDINAL,
                                              &actual_type,
                                              &actual_format,
                                              &item_count,
                                              &bytes_after,
                                              &property);

        rect work_area;
        if (result == Success && actual_type == XA_CARDINAL &&
            actual_format == 32 && item_count >= 4) {
            auto *values = reinterpret_cast<unsigned long *>(property);
            work_area = rect(static_cast<coord>(values[0]),
                             static_cast<coord>(values[1]),
                             static_cast<dim>(values[2]),
                             static_cast<dim>(values[3]));
        }

        if (property)
            XFree(property);

        return work_area;
    }

    const std::vector<screen> &screen::detect() {
        _screens.clear();

        if (!linux::x11::cached_display) {
            linux::x11::cached_display = XOpenDisplay(nullptr);
            if (!linux::x11::cached_display)
                throw std::runtime_error(
                    "X11: No display is available.");
        }

        Display *display = linux::x11::cached_display;
        Window root = DefaultRootWindow(display);

        XRRScreenResources *res =
            XRRGetScreenResourcesCurrent(display, root);
        if (!res)
            throw std::runtime_error(
                "X11: Failed to query XRandR screen resources.");

        RROutput primary_output = XRRGetOutputPrimary(display, root);
        rect advertised_work_area =
            get_work_area_if_supported(display, root);
        std::vector<screen> detected;

        for (int i = 0; i < res->noutput; ++i) {
            RROutput output = res->outputs[i];
            XRROutputInfo *output_info =
                XRRGetOutputInfo(display, res, output);
            if (!output_info) {
                XRRFreeScreenResources(res);
                throw std::runtime_error(
                    "X11: Failed to query XRandR output information.");
            }

            if (output_info->connection != RR_Connected ||
                output_info->crtc == 0) {
                XRRFreeOutputInfo(output_info);
                continue;
            }

            XRRCrtcInfo *crtc_info =
                XRRGetCrtcInfo(display, res, output_info->crtc);
            if (!crtc_info) {
                XRRFreeOutputInfo(output_info);
                XRRFreeScreenResources(res);
                throw std::runtime_error(
                    "X11: Failed to query XRandR display geometry.");
            }

            rect bounds(crtc_info->x,
                        crtc_info->y,
                        crtc_info->width,
                        crtc_info->height);

            bool is_primary = (output == primary_output);
            detected.emplace_back(static_cast<int>(detected.size()),
                                  bounds,
                                  advertised_work_area,
                                  is_primary);

            XRRFreeCrtcInfo(crtc_info);
            XRRFreeOutputInfo(output_info);
        }

        XRRFreeScreenResources(res);
        _screens = std::move(detected);
        normalize();
        return _screens;
    }
} // namespace native
