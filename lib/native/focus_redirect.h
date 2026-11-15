//
// Stores a composite control's private focus destination in its own peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <functional>
namespace native::detail
{
    struct focus_redirect
    {
        std::function<void()> apply;
    };
}
