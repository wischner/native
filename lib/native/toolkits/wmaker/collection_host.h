//
// Declares the shared WINGs frame host for native-look virtual
// collection controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

namespace native
{
    class wnd;
}

namespace linux::wmaker
{
    struct native_collection;

    // Create a WINGs frame which routes collection input and painting.
    native_collection *create_collection_frame(native::wnd &owner);

    // Destroy a WINGs collection frame and notify its owner.
    void destroy_collection_frame(native::wnd &owner,
                                  native_collection *state);
} // namespace linux::wmaker
