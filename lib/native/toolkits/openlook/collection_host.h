//
// Declares the shared XView Panel host used by custom OPEN LOOK
// collection controls and their OLGX-aware painters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

namespace native
{
    class wnd;
}

namespace linux::openlook
{
    struct openlook_collection;

    // Create a hidden focusable XView collection Panel.
    openlook_collection *create_collection_panel(native::wnd &owner);

    // Destroy an XView collection Panel and notify its owner.
    void destroy_collection_panel(native::wnd &owner,
                                  openlook_collection *state);
} // namespace linux::openlook
