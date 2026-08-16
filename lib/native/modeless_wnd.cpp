//
// Implements backend-neutral modeless owned-window construction.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/modeless_wnd.h>

#include <utility>

namespace native
{
    modeless_wnd::modeless_wnd(app_wnd &owner,
                               std::string title,
                               coord x,
                               coord y,
                               dim width,
                               dim height)
        : owned_wnd(
              owner, std::move(title), x, y, width, height) {}

    modeless_wnd::modeless_wnd(app_wnd &owner,
                               const std::string &title,
                               const point &position,
                               const size &dimensions)
        : modeless_wnd(owner,
                       title,
                       position.x,
                       position.y,
                       dimensions.w,
                       dimensions.h) {}

    modeless_wnd::modeless_wnd(app_wnd &owner,
                               const std::string &title,
                               const rect &bounds)
        : modeless_wnd(owner, title, bounds.p, bounds.d) {}
} // namespace native
