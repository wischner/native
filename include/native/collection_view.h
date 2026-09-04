//
// Declares the common base of indexed, keyboard-navigable collection
// controls while leaving each collection's model and navigation API distinct.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "custom_control.h"

namespace native
{
    // Identifies a painted, focusable collection control.
    class collection_view : public custom_control
    {
    public:
        // Destroy common collection-view state.
        ~collection_view() override;

        // Return the vertical item-space scroll offset.
        int get_scroll_offset() const;

        // Clamp and apply a vertical item-space scroll offset.
        virtual collection_view &set_scroll_offset(int offset);

    protected:
        using custom_control::custom_control;

        // Return the largest valid vertical item-space offset.
        virtual int maximum_scroll_offset() const;

        // Apply the cached vertical offset to a native or painted peer.
        virtual void apply_scroll_offset();

        int _scroll_offset = 0;
    };
} // namespace native
