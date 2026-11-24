//
// Lets a backend synchronize native focus eligibility when tab order changes.
// Only backends that need per-item keyboard registration install this state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <functional>

namespace native::detail
{
    struct tab_order_binding
    {
        std::function<void()> apply;
    };
}
