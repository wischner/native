//
// Declares native Athena scrollbar hosting for painted control peers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Intrinsic.h>
#include <native.h>

namespace linux::x11
{
    struct xaw_scroll_axis
    {
        native::wnd *owner = nullptr;
        Widget widget = nullptr;
        bool horizontal = false;
        std::uint64_t total = 0;
        std::uint64_t page = 0;
        std::uint64_t position = 0;
        std::int64_t origin = 0;
        int length = 1;
    };

    // Widgets belong to the Xt host; callback state belongs to its peer.
    struct xaw_scrollbars
    {
        xaw_scroll_axis horizontal;
        xaw_scroll_axis vertical;
    };

    // True for controls whose scroll chrome is supplied by native children.
    bool has_native_scrollbars(const native::wnd &owner);

    // Reconcile visible tracks, range and thumb with the portable viewport.
    void synchronize_scrollbars(native::wnd &owner, Widget host);
}
