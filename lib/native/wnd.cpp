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
#include <native/non_client.h>
#include <native/wnd.h>

#include "wnd_peer.h"

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
        , _parent(nullptr)
        , _mouse_screen_position(x, y) {}

    wnd::wnd(const point &position, const size &dimensions)
        : wnd(position.x, position.y, dimensions.w, dimensions.h) {}

    wnd::wnd(const rect &bounds)
        : wnd(bounds.p, bounds.d) {}

    wnd::~wnd() {
        for (non_client *element : _non_client) {
            if (element && element->_owner == this)
                element->_owner = nullptr;
        }
        _non_client.clear();

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
        for (non_client *element : _non_client) {
            if (element)
                element->on_configuration_changed();
        }
        return *this;
    }

    rect wnd::get_bounds() const {
        return _bounds;
    }

    rect wnd::get_chrome_bounds() const {
        return rect(0, 0, _bounds.d.w, _bounds.d.h);
    }

    rect wnd::reserve_non_client(const rect &available) const {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        for (const non_client *element : _non_client) {
            if (!element || !element->_visible)
                continue;
            switch (element->_edge) {
            case window_edge::top:
                top += element->_extent;
                break;
            case window_edge::right:
                right += element->_extent;
                break;
            case window_edge::bottom:
                bottom += element->_extent;
                break;
            case window_edge::left:
                left += element->_extent;
                break;
            }
        }

        const int width = std::max(0,
            static_cast<int>(available.d.w) - left - right);
        const int height = std::max(0,
            static_cast<int>(available.d.h) - top - bottom);
        return rect(static_cast<coord>(available.p.x + left),
                    static_cast<coord>(available.p.y + top),
                    static_cast<dim>(width),
                    static_cast<dim>(height));
    }

    rect wnd::get_client_bounds() const {
        return reserve_non_client(get_chrome_bounds());
    }

    wnd &wnd::set_bounds(const rect &bounds) {
        {
            const scoped_flag pass(_layout_suspended);
            _bounds = bounds;
            if (_created && _peer)
                _peer->apply_bounds(_bounds);
        }

        relayout_children();
        on_bounds_changed();
        for (non_client *element : _non_client) {
            if (element)
                element->on_configuration_changed();
        }
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

    bool wnd::get_visible() const {
        return _visible;
    }

    mouse_cursor wnd::get_cursor() const {
        return _cursor;
    }

    wnd &wnd::set_cursor(mouse_cursor cursor) {
        if (_cursor == cursor)
            return *this;

        _cursor = cursor;
        if (_created)
            apply_cursor();
        return *this;
    }

    bool wnd::get_input_enabled() const {
        const wnd *root = this;
        while (root->_parent)
            root = root->_parent;

        const auto *window = dynamic_cast<const app_wnd *>(root);
        return !window || window->get_input_enabled();
    }

    void wnd::create() {
        if (_created)
            return;

        if (!dynamic_cast<app_wnd *>(this)) {
            if (!_parent)
                throw std::logic_error(
                    "A child window requires a parent before create().");
            if (!_parent->get_created())
                throw std::logic_error(
                    "A child window requires a created parent.");
        }

        _created = true;
        _peer = detail::create_wnd_peer(*this);
        try {
            create_native();
            apply_cursor();
        } catch (...) {
            _peer.reset();
            _created = false;
            throw;
        }
        on_native_create();
    }

    void wnd::show() {
        if (!_created)
            throw std::logic_error(
                "A window must be created before show().");
        _peer->apply_visible(true);
        // Synchronous system panels (file, directory, and similar dialogs)
        // can complete and destroy their logical resource inside show_native.
        // Their visibility transition has already finished in that case.
        if (!_created || !_peer)
            return;
        apply_cursor();
        _visible = true;
    }

    void wnd::destroy() {
        if (!_created)
            return;

        _destroying = true;
        try {
            destroy_native();
        } catch (...) {
            _destroying = false;
            if (!_created)
                _peer.reset();
            throw;
        }
        _destroying = false;
        if (_created)
            on_native_destroy();
        else
            _peer.reset();
    }

    void wnd::on_native_create() {
        on_wnd_create.emit();
    }

    void wnd::on_native_move(const point &position) {
        _bounds.p = position;
        on_wnd_move.emit(position);
    }

    void wnd::on_native_destroy() {
        if (!_created)
            return;

        destroy_children();
        delete _gpx;
        _gpx = nullptr;
        if (!_destroying)
            _peer.reset();
        _visible = false;
        _created = false;
    }

    void wnd::on_native_resize(const size &dimensions) {
        if (_bounds.d.w != dimensions.w ||
            _bounds.d.h != dimensions.h) {
            _bounds.d = dimensions;
            relayout_children();
            on_bounds_changed();
        }
        for (non_client *element : _non_client) {
            if (element)
                element->on_configuration_changed();
        }
        on_wnd_resize.emit(dimensions);
    }

    void wnd::on_native_paint(wnd_paint_event event) {
        on_wnd_paint.emit(event);
        draw_non_client(event.g);
    }

    void wnd::draw_non_client(gpx &graphics) {
        for (non_client *element : _non_client) {
            if (element && element->_visible)
                element->draw(graphics, non_client_bounds(element));
        }
    }

    void wnd::on_native_mouse_move(const point &position) {
        if (!_mouse_screen_position_exact) {
            point screen_position = position;
            const wnd *current = this;
            while (current) {
                screen_position.x = static_cast<coord>(
                    screen_position.x + current->get_position().x);
                screen_position.y = static_cast<coord>(
                    screen_position.y + current->get_position().y);
                current = current->get_parent();
            }
            _mouse_screen_position = screen_position;
        }
        _mouse_screen_position_exact = false;
        on_mouse_move.emit(position);
        for (non_client *element : _non_client) {
            if (element && element->_visible)
                element->track_pointer(position);
        }
    }

    void wnd::on_native_mouse_move(
        const point &position,
        const point &screen_position) {
        _mouse_screen_position = screen_position;
        _mouse_screen_position_exact = true;
        on_native_mouse_move(position);
    }

    point wnd::get_mouse_screen_position() const {
        return _mouse_screen_position;
    }

    void wnd::on_native_focus(bool) {}

    wnd &wnd::invalidate() {
        return invalidate(
            rect(point(0, 0), get_dimensions()));
    }

    wnd &wnd::invalidate(const rect &invalid) {
        if (_created && _peer)
            _peer->invalidate(invalid);
        return *this;
    }

    void wnd::on_native_mouse_click(mouse_event event) {
        on_mouse_click.emit(event);
    }

    void wnd::on_native_mouse_wheel(mouse_wheel_event event) {
        on_mouse_wheel.emit(event);
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
        _layout->relayout(this, get_client_bounds());
    }

    void wnd::attach_non_client(non_client *element) {
        if (!element || std::find(_non_client.begin(),
                                  _non_client.end(), element) !=
                            _non_client.end())
            return;
        _non_client.push_back(element);
        relayout_children();
        invalidate();
    }

    void wnd::detach_non_client(non_client *element) {
        const auto found = std::find(_non_client.begin(),
                                     _non_client.end(), element);
        if (found == _non_client.end())
            return;
        _non_client.erase(found);
        relayout_children();
        invalidate();
    }

    rect wnd::non_client_bounds(const non_client *target) const {
        int left_total = 0;
        int top_total = 0;
        int right_total = 0;
        int bottom_total = 0;
        for (const non_client *element : _non_client) {
            if (!element || !element->_visible)
                continue;
            switch (element->_edge) {
            case window_edge::top: top_total += element->_extent; break;
            case window_edge::right: right_total += element->_extent; break;
            case window_edge::bottom: bottom_total += element->_extent; break;
            case window_edge::left: left_total += element->_extent; break;
            }
        }

        int before = 0;
        for (const non_client *element : _non_client) {
            if (element == target)
                break;
            if (element && element->_visible &&
                element->_edge == target->_edge)
                before += element->_extent;
        }

        // Strips stack inside the area this window's own edge chrome
        // leaves behind, so a canvas ruler stops at its scrollbars
        // instead of running under them.
        const rect chrome = get_chrome_bounds();
        const int origin_x = chrome.p.x;
        const int origin_y = chrome.p.y;
        const int width = static_cast<int>(chrome.d.w);
        const int height = static_cast<int>(chrome.d.h);
        switch (target->_edge) {
        case window_edge::top:
            return rect(static_cast<coord>(origin_x+left_total),
                        static_cast<coord>(origin_y+before),
                        static_cast<dim>(std::max(0, width-left_total-right_total)),
                        static_cast<dim>(target->_extent));
        case window_edge::right:
            return rect(static_cast<coord>(origin_x+width-before-target->_extent),
                        static_cast<coord>(origin_y+top_total),
                        static_cast<dim>(target->_extent),
                        static_cast<dim>(std::max(0, height-top_total-bottom_total)));
        case window_edge::bottom:
        {
            const bool spans_window =
                dynamic_cast<const status_bar *>(target) != nullptr;
            return rect(static_cast<coord>(origin_x + (spans_window ? 0 : left_total)),
                        static_cast<coord>(origin_y+height-before-target->_extent),
                        static_cast<dim>(spans_window ? width :
                            std::max(0, width-left_total-right_total)),
                        static_cast<dim>(target->_extent));
        }
        case window_edge::left:
            return rect(static_cast<coord>(origin_x+before),
                        static_cast<coord>(origin_y+top_total),
                        static_cast<dim>(target->_extent),
                        static_cast<dim>(std::max(0, height-top_total-bottom_total)));
        }
        return {};
    }

    void wnd::on_bounds_changed() {}

    void wnd::destroy_children() {
        for (wnd *child : _children) {
            if (child && child->_created)
                child->destroy();
        }
    }

} // namespace native
