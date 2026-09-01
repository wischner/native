//
// Declares the shared Haiku collection-control host view.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

class BView;

namespace native
{
    class wnd;
}

namespace haiku
{
    BView *create_collection_view(native::wnd &owner);
}
