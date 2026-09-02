//
// Implements shared edge-attached non-client window elements.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native/non_client.h>
#include <native/wnd.h>

namespace native
{
    non_client::non_client(wnd &owner, window_edge edge, int extent)
        : _owner(&owner)
        , _edge(edge)
        , _extent(extent)
        , _visible(true) {
        if (extent < 0)
            throw std::invalid_argument(
                "A non-client extent cannot be negative.");
        _owner->attach_non_client(this);
    }

    non_client::~non_client() {
        if (_owner)
            _owner->detach_non_client(this);
    }

    wnd *non_client::get_owner() const {
        return _owner;
    }

    window_edge non_client::get_edge() const {
        return _edge;
    }

    non_client &non_client::set_edge(window_edge edge) {
        if (_edge == edge)
            return *this;
        _edge = edge;
        if (_owner) {
            _owner->relayout_children();
            _owner->invalidate();
        }
        return *this;
    }

    int non_client::get_extent() const {
        return _extent;
    }

    non_client &non_client::set_extent(int extent) {
        if (extent < 0)
            throw std::invalid_argument(
                "A non-client extent cannot be negative.");
        if (_extent == extent)
            return *this;
        _extent = extent;
        if (_owner) {
            _owner->relayout_children();
            _owner->invalidate();
        }
        return *this;
    }

    bool non_client::get_visible() const {
        return _visible;
    }

    non_client &non_client::set_visible(bool visible) {
        if (_visible == visible)
            return *this;
        _visible = visible;
        if (_owner) {
            _owner->relayout_children();
            _owner->invalidate();
        }
        return *this;
    }

    rect non_client::get_bounds() const {
        return _owner ? _owner->non_client_bounds(this) : rect();
    }

    void non_client::track_pointer(const point &) {}

    void non_client::invalidate() const {
        if (_owner && _visible)
            _owner->invalidate(get_bounds());
    }
} // namespace native
