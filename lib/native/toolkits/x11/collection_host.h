//
// Declares the shared Athena drawable host for collection controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <X11/Intrinsic.h>

namespace native
{
    class wnd;
}

namespace linux::x11
{
    // Create an unmanaged Form which paints and routes collection input.
    Widget create_collection_host(native::wnd &owner);
}
