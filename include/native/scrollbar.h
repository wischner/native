//
// Declares the scrollbar visibility policy shared by every scrollable
// native control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

namespace native
{
    // Selects automatic, permanently shown, or hidden scrollbars.
    enum class scrollbar_policy
    {
        automatic,
        always,
        never
    };
} // namespace native
