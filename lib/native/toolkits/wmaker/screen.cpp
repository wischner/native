//
// Detects Window Maker displays through XRandR and the EWMH work area.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <native/screen.h>

#include "globals.h"

namespace
{
    native::rect advertised_work_area(Display *display,
                                      Window root) {
        const Atom workarea = XInternAtom(
            display, "_NET_WORKAREA", True);
        if (workarea == None)
            return {};
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char *property = nullptr;
        const int status = XGetWindowProperty(display,
                                              root,
                                              workarea,
                                              0,
                                              4,
                                              False,
                                              XA_CARDINAL,
                                              &type,
                                              &format,
                                              &count,
                                              &remaining,
                                              &property);
        native::rect result;
        if (status == Success && type == XA_CARDINAL &&
            format == 32 && count >= 4) {
            auto *values =
                reinterpret_cast<unsigned long *>(property);
            result = native::rect(
                static_cast<native::coord>(values[0]),
                static_cast<native::coord>(values[1]),
                static_cast<native::dim>(values[2]),
                static_cast<native::dim>(values[3]));
        }
        if (property)
            XFree(property);
        return result;
    }
} // namespace

namespace native
{
    const std::vector<screen> &screen::detect() {
        _screens.clear();
        if (!linux::wmaker::display) {
            linux::wmaker::display = XOpenDisplay(nullptr);
            if (!linux::wmaker::display) {
                throw std::runtime_error(
                    "Window Maker/WINGs: no display is available.");
            }
        }
        Display *display = linux::wmaker::display;
        const Window root = DefaultRootWindow(display);
        XRRScreenResources *resources =
            XRRGetScreenResourcesCurrent(display, root);
        if (!resources) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to query XRandR.");
        }
        const RROutput primary = XRRGetOutputPrimary(display, root);
        const rect work_area = advertised_work_area(display, root);
        std::vector<screen> detected;
        for (int index = 0; index < resources->noutput; ++index) {
            const RROutput output = resources->outputs[index];
            XRROutputInfo *output_info = XRRGetOutputInfo(
                display, resources, output);
            if (!output_info) {
                XRRFreeScreenResources(resources);
                throw std::runtime_error(
                    "Window Maker/WINGs: invalid XRandR output.");
            }
            if (output_info->connection != RR_Connected ||
                output_info->crtc == 0) {
                XRRFreeOutputInfo(output_info);
                continue;
            }
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(
                display, resources, output_info->crtc);
            if (!crtc) {
                XRRFreeOutputInfo(output_info);
                XRRFreeScreenResources(resources);
                throw std::runtime_error(
                    "Window Maker/WINGs: invalid XRandR geometry.");
            }
            const rect bounds(
                crtc->x, crtc->y, crtc->width, crtc->height);
            detected.emplace_back(
                static_cast<int>(detected.size()),
                bounds,
                work_area,
                output == primary);
            XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(output_info);
        }
        XRRFreeScreenResources(resources);
        _screens = std::move(detected);
        normalize();
        return _screens;
    }
} // namespace native
