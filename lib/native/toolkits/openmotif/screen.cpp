//
// Implements the OpenMotif display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <native.h>
#include <native/screen.h>

#include "globals.h"

namespace native
{
    // Return the work area advertised by an EWMH window manager.
    static rect get_work_area(Display *display, Window root) {
        Atom work_area_atom =
            XInternAtom(display, "_NET_WORKAREA", True);
        if (work_area_atom == None)
            return {};

        Atom actual_type = None;
        int actual_format = 0;
        unsigned long item_count = 0;
        unsigned long bytes_after = 0;
        unsigned char *property = nullptr;

        const int result = XGetWindowProperty(display,
                                              root,
                                              work_area_atom,
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

        if (!linux::openmotif::cached_display) {
            linux::openmotif::cached_display = XOpenDisplay(nullptr);
            if (!linux::openmotif::cached_display)
                throw std::runtime_error(
                    "Motif: No display is available.");
        }

        Display *display = linux::openmotif::cached_display;
        const int screen_count = ScreenCount(display);

        for (int i = 0; i < screen_count; ++i) {
            Screen *scr = ScreenOfDisplay(display, i);
            rect bounds(0, 0, WidthOfScreen(scr), HeightOfScreen(scr));
            rect work_area =
                get_work_area(display, RootWindowOfScreen(scr));
            bool is_primary = (i == DefaultScreen(display));
            _screens.emplace_back(i, bounds, work_area, is_primary);
        }

        normalize();
        return _screens;
    }

} // namespace native
