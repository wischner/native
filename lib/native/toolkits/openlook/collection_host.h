//
// Declares the shared XView Panel host used by custom OPEN LOOK
// collection controls and their OLGX-aware painters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/geometry.h>

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

    // Paint an invalid collection region into its backing pixmap and copy
    // it to the live XView paint window without first exposing a cleared
    // Panel.  Docking overlays use this path while their hot state changes.
    void repaint_collection(native::wnd &owner,
                            const native::rect &area);

    // Resize a collection Panel and refresh the paint-window event and
    // owner bindings if XView replaces that view during the resize.
    void resize_collection_panel(native::wnd &owner,
                                 openlook_collection &state,
                                 const native::size &dimensions);
} // namespace linux::openlook
