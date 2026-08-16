//
// Implements the backend-neutral owner graph for independent top-level
// windows. Ownership is separate from child layout and C++ ownership.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/owned_wnd.h>

#include <utility>

namespace native
{
    owned_wnd::owned_wnd(app_wnd &owner,
                         std::string title,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : app_wnd(std::move(title), x, y, width, height)
        , _owner(&owner) {
        _owner->attach_owned_window(this);
    }

    owned_wnd::owned_wnd(app_wnd &owner,
                         const std::string &title,
                         const point &position,
                         const size &dimensions)
        : owned_wnd(owner,
                    title,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    owned_wnd::owned_wnd(app_wnd &owner,
                         const std::string &title,
                         const rect &bounds)
        : owned_wnd(owner, title, bounds.p, bounds.d) {}

    owned_wnd::~owned_wnd() {
        destroy();
        detach_owner();
    }

    app_wnd *owned_wnd::get_owner() const {
        return _owner;
    }

    void owned_wnd::detach_owner() {
        if (!_owner)
            return;

        app_wnd *owner = _owner;
        _owner = nullptr;
        owner->detach_owned_window(this);
    }
} // namespace native
