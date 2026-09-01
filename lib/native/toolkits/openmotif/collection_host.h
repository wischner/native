//
// Declares the shared focusable Motif host used by virtual collection
// controls when no suitable materialized widget path applies.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <Xm/Xm.h>

namespace native
{
    class wnd;
}

namespace linux::openmotif
{
    struct motif_collection;

    // Create an unmanaged Motif drawing-area collection host.
    Widget create_collection_host(native::wnd &owner);

    // Destroy a collection host and notify its portable owner.
    void destroy_collection_host(native::wnd &owner,
                                 motif_collection *state);
} // namespace linux::openmotif
