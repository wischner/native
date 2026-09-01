//
// Implements backend-neutral window state and hierarchy behavior.
// Backends apply cached state to their native resources through hooks.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>
#include <native/wnd.h>

namespace
{
    // Holds a flag true for the lifetime of a scope and restores the
    // previous value on the way out, including when the guarded code
    // throws.
    class scoped_flag
    {
    public:
        explicit scoped_flag(bool &flag)
            : _flag(flag)
            , _previous(flag) {
            _flag = true;
        }

        ~scoped_flag() {
            _flag = _previous;
        }

        scoped_flag(const scoped_flag &) = delete;
        scoped_flag &operator=(const scoped_flag &) = delete;

    private:
        bool &_flag;
        bool _previous;
    };
} // namespace

namespace native
{
    wnd::wnd(coord x, coord y, dim width, dim height)
        : _created(false)
        , _bounds(x, y, width, height)
        , _parent(nullptr) {}

    wnd::wnd(const point &position, const size &dimensions)
        : wnd(position.x, position.y, dimensions.w, dimensions.h) {}

    wnd::wnd(const rect &bounds)
        : wnd(bounds.p, bounds.d) {}

    wnd::~wnd() {
        if (_parent) {
            wnd *old_parent = _parent;
            _parent = nullptr;
            old_parent->_children.erase(
                std::remove(old_parent->_children.begin(),
                            old_parent->_children.end(),
                            this),
                old_parent->_children.end());

            if (old_parent->_layout) {
                old_parent->_layout->remove_child(this);
                old_parent->relayout_children();
            }
        }

        for (wnd *child : _children) {
            if (child && child->_parent == this)
                child->_parent = nullptr;
        }

        delete _gpx;
    }

    point wnd::get_position() const {
        return _bounds.p;
    }

    wnd &wnd::set_position(const point &position) {
        _bounds.p = position;
        if (_created)
            apply_position();
        return *this;
    }

    size wnd::get_dimensions() const {
        return _bounds.d;
    }

    wnd &wnd::set_dimensions(const size &dimensions) {
        {
            // Applying geometry can come straight back as a native
            // resize notification carrying the size the backend
            // actually granted. Suspending the pass lets that update
            // the cache, so the single pass below arranges children
            // against the granted size rather than the requested one.
            const scoped_flag pass(_layout_suspended);
            _bounds.d = dimensions;
            if (_created)
                apply_dimensions();
        }

        relayout_children();
        on_bounds_changed();
        return *this;
    }

    rect wnd::get_bounds() const {
        return _bounds;
    }

    wnd &wnd::set_bounds(const rect &bounds) {
        {
            const scoped_flag pass(_layout_suspended);
            _bounds = bounds;
            if (_created)
                apply_bounds();
        }

        relayout_children();
        on_bounds_changed();
        return *this;
    }

    wnd *wnd::get_parent() const {
        return _parent;
    }

    wnd &wnd::set_parent(wnd *parent) {
        if (_parent == parent)
            return *this;

        if (_created && !dynamic_cast<app_wnd *>(this) && !parent)
            throw std::invalid_argument(
                "A created control requires a parent.");

        for (wnd *ancestor = parent; ancestor;
             ancestor = ancestor->_parent) {
            if (ancestor == this)
                throw std::invalid_argument(
                    "A window cannot parent itself or an ancestor.");
        }

        if (_created && parent && !parent->_created)
            throw std::invalid_argument(
                "A created window requires a created parent.");

        wnd *old_parent = _parent;
        if (old_parent) {
            old_parent->_children.erase(
                std::remove(old_parent->_children.begin(),
                            old_parent->_children.end(),
                            this),
                old_parent->_children.end());

            if (old_parent->_layout) {
                old_parent->_layout->remove_child(this);
                old_parent->relayout_children();
            }
        }

        _parent = parent;

        if (_parent) {
            const auto child = std::find(_parent->_children.begin(),
                                         _parent->_children.end(),
                                         this);
            if (child == _parent->_children.end())
                _parent->_children.push_back(this);

            if (_parent->_layout) {
                _parent->_layout->add_child(this);
                _parent->relayout_children();
            }
        }

        if (_created)
            apply_parent();

        return *this;
    }

    bool wnd::get_created() const {
        return _created;
    }

    bool wnd::get_input_enabled() const {
        const wnd *root = this;
        while (root->_parent)
            root = root->_parent;

        const auto *window = dynamic_cast<const app_wnd *>(root);
        return !window || window->get_input_enabled();
    }

    void wnd::on_native_move(const point &position) {
        _bounds.p = position;
    }

    void wnd::on_native_destroy() {
        if (!_created)
            return;

        destroy_children();
        delete _gpx;
        _gpx = nullptr;
        _created = false;
    }

    void wnd::on_native_resize(const size &dimensions) {
        if (_bounds.d.w == dimensions.w && _bounds.d.h == dimensions.h)
            return;

        _bounds.d = dimensions;
        relayout_children();
        on_bounds_changed();
    }

    wnd &wnd::set_layout(std::unique_ptr<layout_manager> layout) {
        _layout = std::move(layout);
        if (_layout) {
            for (wnd *child : _children)
                _layout->add_child(child);
            relayout_children();
        }
        return *this;
    }

    layout_manager *wnd::get_layout() const {
        return _layout.get();
    }

    void wnd::relayout_children() {
        if (!_layout || _layout_suspended)
            return;

        const scoped_flag pass(_layout_suspended);
        _layout->relayout(this, rect({0, 0}, _bounds.d));
    }

    void wnd::on_bounds_changed() {}

    void wnd::destroy_children() const {
        for (wnd *child : _children) {
            if (child && child->_created)
                child->destroy();
        }
    }

} // namespace native
