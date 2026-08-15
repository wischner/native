//
// Implements backend-independent access to a window graphics context.
// Drawing and resource management remain in the selected backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "gpx_wnd.h"

namespace native
{
    wnd *gpx_wnd::window() const {
        return _wnd;
    }
}
