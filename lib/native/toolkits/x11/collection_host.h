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
    //
    // Create an unmanaged Form which paints and routes owner input.
    //
    // Notes:
    //      Collections, canvases, and any other portable control whose
    //      client pixels are painted by Native share this host. The
    //      widget name only reaches Xt resource lookups.
    //
    Widget create_collection_host(native::wnd &owner,
                                  const char *name = "collection");
}
