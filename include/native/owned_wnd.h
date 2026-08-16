//
// Declares the shared base for independently positioned windows that
// remain owned by another top-level window.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "app_wnd.h"

namespace native
{
    // Represents an owned top-level window outside parent layout.
    class owned_wnd : public app_wnd
    {
    public:
        // Destroy the native resource and detach from the owner graph.
        ~owned_wnd() override;

        // Return the non-owning top-level owner, if it is still alive.
        app_wnd *get_owner() const override;

    protected:
        // Construct an owned window from scalar screen bounds.
        owned_wnd(app_wnd &owner,
                  std::string title,
                  coord x = 100,
                  coord y = 100,
                  dim width = 640,
                  dim height = 480);

        // Construct an owned window from screen position and size.
        owned_wnd(app_wnd &owner,
                  const std::string &title,
                  const point &position,
                  const size &dimensions);

        // Construct an owned window from complete screen bounds.
        owned_wnd(app_wnd &owner,
                  const std::string &title,
                  const rect &bounds);

    private:
        friend class app_wnd;

        app_wnd *_owner;

        // Clear the owner link when either C++ object is destroyed.
        void detach_owner();
    };
} // namespace native
