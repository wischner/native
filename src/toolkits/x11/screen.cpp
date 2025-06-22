#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include <native.h>
#include "globals.h"

namespace native
{
    // --- Read EWMH _NET_WORKAREA property for root window. ---------
    static rect get_work_area_if_supported(Display *display, Window root)
    {
        Atom net_workarea = XInternAtom(display, "_NET_WORKAREA", True);
        if (net_workarea == None)
            return rect(); // invalid

        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;

        if (Success == XGetWindowProperty(
                           display,
                           root,
                           net_workarea,
                           0,
                           4 * 32, // read up to 32 monitors if available
                           False,
                           XA_CARDINAL,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           &prop))
        {
            if (nitems >= 4 && actual_format == 32)
            {
                long *values = reinterpret_cast<long *>(prop);

                // Take first work area entry (most WMs only report one)
                rect r(values[0], values[1], values[2], values[3]);
                XFree(prop);
                return r;
            }
        }
        if (prop)
            XFree(prop);

        return rect(); // invalid
    }

    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        if (!x11::cached_display)
        {
            x11::cached_display = XOpenDisplay(nullptr);
            if (!x11::cached_display)
                throw std::runtime_error("X11: No display available to detect screens.");
        }

        Display *display = x11::cached_display;
        Window root = DefaultRootWindow(display);

        XRRScreenResources *res = XRRGetScreenResourcesCurrent(display, root);
        if (!res)
            throw std::runtime_error("X11: Failed to get current XRandR screen resources.");

        RROutput primary_output = XRRGetOutputPrimary(display, root);

        for (int i = 0; i < res->noutput; ++i)
        {
            RROutput output = res->outputs[i];
            XRROutputInfo *output_info = XRRGetOutputInfo(display, res, output);
            if (!output_info)
                continue;

            if (output_info->connection != RR_Connected || output_info->crtc == 0)
            {
                XRRFreeOutputInfo(output_info);
                continue;
            }

            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, res, output_info->crtc);
            if (!crtc_info)
            {
                XRRFreeOutputInfo(output_info);
                continue;
            }

            rect bounds(
                crtc_info->x,
                crtc_info->y,
                crtc_info->width,
                crtc_info->height);

            bool is_primary = (output == primary_output);
            bool is_landscape = crtc_info->width >= crtc_info->height;

            rect work_area = bounds;

            if (is_primary)
            {
                rect wa = get_work_area_if_supported(display, root);

                if (is_primary && wa.w() > 0 && wa.h() > 0)
                {
                    rect clipped = wa.intersect(bounds);
                    if (clipped.w() > 0 && clipped.h() > 0)
                        work_area = clipped;
                }
            }

            _screens.emplace_back(i, bounds, work_area, is_primary /* , is_landscape */);

            XRRFreeCrtcInfo(crtc_info);
            XRRFreeOutputInfo(output_info);
        }

        XRRFreeScreenResources(res);
        return _screens;
    }
} // namespace native
