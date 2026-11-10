//
// Chooses the docking host for a scrolling document's status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../status_bar_peer.h"
#include <native/canvas.h>
#include "globals.h"

#include <algorithm>

namespace native::detail
{
    rect status_bar_bounds(const wnd &owner, const rect &bounds) {
        const WORD handle = linux::gemix::wnd_bindings.handle_from_object(
            const_cast<wnd *>(&owner));
        if (handle <= 0 || !bounds.w() || !bounds.h())
            return bounds;
        WORD kind = 0, unused = 0;
        if (!wind_get(handle, WF_KIND, &kind, &unused, &unused, &unused) ||
            !(kind & SIZER))
            return bounds;

        // Ask AES for its full-size corner span. With compact sizing,
        // part of that square lies inside the work rectangle. Windows
        // with native scroll tracks already exclude the entire corner.
        const auto outer = linux::gemix::outer_rect(handle);
        const auto work = linux::gemix::work_rect(handle);
        WORD x = 0, y = 0, width = 0, height = 0;
        if (!wind_calc(WC_WORK, kind | VSLIDE | HSLIDE,
                       outer.x1(), outer.y1(), outer.w(), outer.h(),
                       &x, &y, &width, &height))
            return bounds;
        const int span = outer.x2() - (x + width);
        if (work.y1() + bounds.y2() <= outer.y2() - span)
            return bounds;
        const int right = std::min<int>(bounds.x2(),
            outer.x2() - span - work.x1());
        return rect(bounds.x1(), bounds.y1(),
                    std::max(0, right - bounds.x1()), bounds.h());
    }

    wnd &status_bar_host(wnd &, canvas &content) { return content; }
}
