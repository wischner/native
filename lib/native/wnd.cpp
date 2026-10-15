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
#include "input_state.h"
#include "mouse_state.h"
#include "emulated_tree.h"

namespace
{
    // Holds a flag true for the lifetime of a scope and restores the
    // previous value on the way out, including when the guarded code
    // throws.
    class scoped_flag
    {
    public:
        scoped_flag(bool &flag, native::wnd &owner)
            : _flag(flag)
            , _previous(flag)
            , _guard(owner) {
            _flag = true;
        }

        ~scoped_flag() {
            if (_guard.object_alive()) _flag = _previous;
        }

        scoped_flag(const scoped_flag &) = delete;
        scoped_flag &operator=(const scoped_flag &) = delete;

    private:
        bool &_flag;
        bool _previous;
        native::detail::window_guard _guard;
    };
} // namespace

namespace native
{
    border_sides wnd::get_border_sides() const {
        return _border_sides;
    }

    wnd &wnd::set_border_sides(border_sides sides) {
        if ((unsigned(sides) & ~unsigned(border_sides::all)) != 0)
            throw std::invalid_argument("Invalid border sides.");
        if (_border_sides == sides) return *this;
        const detail::window_guard guard(*this);
        _border_sides = sides;
        on_border_sides_changed();
        if (!guard.alive()) return *this;
        if (_created) apply_border_sides();
        if (!guard.alive()) return *this;
        invalidate();
        return *this;
    }

    void wnd::on_border_sides_changed() {
        on_bounds_changed();
    }

    wnd::wnd(coord x, coord y, dim width, dim height)
        : _created(false)
        , _bounds(x, y, width, height)
        , _parent(nullptr)
        , _mouse_screen_position(x, y)
        , _object_lifetime(std::make_shared<detail::wnd_lifetime>()) {}

    wnd::wnd(const point &position, const size &dimensions)
        : wnd(position.x, position.y, dimensions.w, dimensions.h) {}

    wnd::wnd(const rect &bounds)
        : wnd(bounds.p, bounds.d) {}

    wnd::~wnd() {
        _object_lifetime->alive = false;
        detail::mouse_access::cancel_tree(*this, mouse_cancel_reason::destroyed, false);
        if (_lifetime) _lifetime->alive = false;
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
        const detail::window_guard guard(*this);
        _bounds.p = position;
        if (_created) {
            apply_position();
            if (!guard.alive()) return *this;
            detail::backend_refresh_mouse(*this);
        }
        return *this;
    }

    size wnd::get_dimensions() const {
        return _bounds.d;
    }

    wnd &wnd::set_dimensions(const size &dimensions) {
        const detail::window_guard guard(*this);
        {
            // Applying geometry can come straight back as a native
            // resize notification carrying the size the backend
            // actually granted. Suspending the pass lets that update
            // the cache, so the single pass below arranges children
            // against the granted size rather than the requested one.
            const scoped_flag pass(_layout_suspended, *this);
            _bounds.d = dimensions;
            if (_created)
                apply_dimensions();
        }

        if (!guard.alive()) return *this;
        relayout_children();
        if (!guard.alive()) return *this;
        on_bounds_changed();
        if (!guard.alive()) return *this;
        if (_created) {
            apply_border_sides();
            if (!guard.alive()) return *this;
            detail::backend_refresh_mouse(*this);
            if (!guard.alive()) return *this;
        }
        const auto elements = _non_client;
        for (non_client *element : elements) {
            if (element && std::find(_non_client.begin(), _non_client.end(),
                                     element) != _non_client.end())
                element->on_configuration_changed();
            if (!guard.alive()) return *this;
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
        const detail::window_guard guard(*this);
        {
            const scoped_flag pass(_layout_suspended, *this);
            _bounds = bounds;
            if (_created && _peer)
                _peer->apply_bounds(_bounds);
        }

        if (!guard.alive()) return *this;
        relayout_children();
        if (!guard.alive()) return *this;
        on_bounds_changed();
        if (!guard.alive()) return *this;
        if (_created) {
            apply_border_sides();
            if (!guard.alive()) return *this;
            detail::backend_refresh_mouse(*this);
            if (!guard.alive()) return *this;
        }
        const auto elements = _non_client;
        for (non_client *element : elements) {
            if (element && std::find(_non_client.begin(), _non_client.end(),
                                     element) != _non_client.end())
                element->on_configuration_changed();
            if (!guard.alive()) return *this;
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

            if (_parent->_layout && !_non_client_surface) {
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

    mouse_cursor wnd::get_cursor_at(const point &position) const {
        return get_input_enabled() && get_client_bounds().contains(position)
            ? _cursor : mouse_cursor::arrow;
    }

    wnd &wnd::set_cursor(mouse_cursor cursor) {
        if (unsigned(cursor) > unsigned(mouse_cursor::hidden))
            throw std::invalid_argument("Invalid mouse cursor.");
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

        const detail::window_guard object(*this);
        _created = true;
        bool native_started = false;
        try {
            _peer = detail::create_wnd_peer(*this);
            _lifetime = std::make_shared<detail::wnd_lifetime>();
            detail::assign_peer_state(*this, new detail::input_state);
            detail::assign_peer_state(*this, new detail::mouse_state);
            native_started = true;
            const detail::window_guard generation(*this);
            create_native();
            if (!generation.alive()) return;
            apply_cursor();
            if (!generation.alive()) return;
            apply_border_sides();
            if (!generation.alive()) return;
        } catch (...) {
            if (!object.object_alive()) throw;
            if (_lifetime) _lifetime->alive = false;
            // Backends accept partial creation and remove any registered
            // native resources while their peer state is still available.
            if (native_started) {
                try { destroy_native(); } catch (...) {}
                if (!object.object_alive()) throw;
            }
            _peer.reset();
            _created = false;
            _visible = false;
            throw;
        }
        const auto weak = detail::mouse_access::lifetime(*this);
        on_native_create();
        const auto lifetime = weak.lock();
        if (!lifetime || !lifetime->alive) return;
        // Internal chrome must be established even when an application
        // consumes the public create signal. Never create child resources
        // from inside the owner's paint transaction.
        const detail::window_guard generation(*this);
        const auto elements = _non_client;
        for (non_client *element : elements) {
            if (element && std::find(_non_client.begin(), _non_client.end(),
                                     element) != _non_client.end())
                element->on_configuration_changed();
            if (!generation.alive()) return;
        }
    }

    void wnd::show() {
        const detail::window_guard guard(*this);
        if (!_created)
            throw std::logic_error(
                "A window must be created before show().");
        _peer->apply_visible(true);
        if (!guard.alive()) return;
        // Synchronous system panels (file, directory, and similar dialogs)
        // can complete and destroy their logical resource inside show_native.
        // Their visibility transition has already finished in that case.
        if (!_created || !_peer)
            return;
        _visible = true;
        apply_cursor();
        if (!guard.alive()) return;
        apply_border_sides();
    }

    void wnd::destroy() {
        if (!_created || _destroying)
            return;

        const detail::window_guard guard(*this);
        _destroying = true;
        detail::mouse_access::cancel_tree(*this, mouse_cancel_reason::destroyed, false);
        if (_lifetime) _lifetime->alive = false;
        try {
            detail::reset_input(*this);
            if (!guard.object_alive()) return;
            destroy_native();
            if (!guard.object_alive()) return;
        } catch (...) {
            if (!guard.object_alive()) throw;
            _destroying = false;
            if (!_created)
                _peer.reset();
            throw;
        }
        _destroying = false;
        if (_created) {
            // Suppress completion until this transaction releases its peer.
            _destroying = true;
            on_native_destroy();
            if (!guard.object_alive()) return;
            _destroying = false;
        }
        _peer.reset();
        on_destroy_completed();
    }

    void wnd::on_native_create() {
        on_wnd_create.emit();
    }

    void wnd::on_native_move(const point &position) {
        _bounds.p = position;
        on_wnd_move.emit(position);
    }

    void wnd::on_native_destroy() {
        if (!_created) return;
        const detail::window_guard guard(*this);
        const bool destroying = _destroying;
        _destroying = true;
        try {
            detail::mouse_access::cancel_tree(*this, mouse_cancel_reason::destroyed, false);
            if (_lifetime) _lifetime->alive = false;
            detail::reset_input(*this);
            if (!guard.object_alive()) return;
            destroy_children();
            if (!guard.object_alive()) return;
            delete _gpx;
            _gpx = nullptr;
            _visible = false;
            _created = false;
            _destroying = destroying;
        } catch (...) {
            if (guard.object_alive()) _destroying = destroying;
            throw;
        }
        if (!destroying) {
            _peer.reset();
            on_destroy_completed();
        }
    }

    void wnd::on_destroy_completed() {}

    void wnd::on_native_resize(const size &dimensions) {
        const detail::window_guard guard(*this);
        if (_bounds.d.w != dimensions.w ||
            _bounds.d.h != dimensions.h) {
            _bounds.d = dimensions;
            relayout_children();
            if (!guard.alive()) return;
            on_bounds_changed();
            if (!guard.alive()) return;
        }
        const auto elements = _non_client;
        for (non_client *element : elements) {
            if (element && std::find(_non_client.begin(), _non_client.end(),
                                     element) != _non_client.end())
                element->on_configuration_changed();
            if (!guard.alive()) return;
        }
        if (_created) apply_border_sides();
        if (!guard.alive()) return;
        on_wnd_resize.emit(dimensions);
    }

    void wnd::on_native_paint(wnd_paint_event event) {
        const detail::window_guard guard(*this);
        on_wnd_paint.emit(event);
        if (guard.alive()) draw_non_client(event.g);
    }

    void wnd::draw_non_client(gpx &graphics) {
        // Perpendicular rulers leave a shared, non-interactive corner.
        // Give that paper its own theme role before drawing either ruler.
        for (non_client *horizontal : _non_client) {
            if (!horizontal || !horizontal->_visible ||
                !dynamic_cast<ruler *>(horizontal) ||
                (horizontal->get_edge() != window_edge::top &&
                 horizontal->get_edge() != window_edge::bottom))
                continue;
            for (non_client *vertical : _non_client) {
                if (!vertical || !vertical->_visible ||
                    !dynamic_cast<ruler *>(vertical) ||
                    (vertical->get_edge() != window_edge::left &&
                     vertical->get_edge() != window_edge::right))
                    continue;
                const auto h = non_client_bounds(horizontal);
                const auto v = non_client_bounds(vertical);
                const rect corner(v.p.x, h.p.y, v.d.w, h.d.h);
                auto appearance = theme::create(graphics);
                appearance->draw_surface(corner, surface_kind::ruler_corner, {});
            }
        }
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
        const bool tracked = static_cast<bool>(_lifetime);
        const auto weak = detail::mouse_access::lifetime(*this);
        wnd *root = detail::root_of(this);
        const point origin = root == this ? point{} : detail::origin_in_root(*this);
        detail::pointer_position(*root, point(
            static_cast<coord>(position.x + origin.x),
            static_cast<coord>(position.y + origin.y)));
        auto lifetime = weak.lock();
        if (tracked && (!lifetime || !lifetime->alive)) return;
        detail::backend_refresh_mouse(*this);
        if (auto *state = detail::peer_state<detail::mouse_state>(*this)) {
            mouse_motion_event motion;
            motion.position = position;
            if (const auto *input = detail::peer_state<detail::input_state>(*this))
                motion.modifiers = input->modifiers;
            if (state->baseline) {
                motion.dx = float(position.x) - state->position.x;
                motion.dy = float(position.y) - state->position.y;
            }
            state->position = position;
            state->baseline = true;
            on_native_mouse_motion(motion);
        }
        lifetime = weak.lock();
        if (tracked && (!lifetime || !lifetime->alive)) return;
        on_mouse_move.emit(position);
        lifetime = weak.lock();
        if (tracked && (!lifetime || !lifetime->alive)) return;
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

    void wnd::on_native_focus(bool focused) {
        wnd *focus_root = detail::root_of(this);
        if (auto *mouse = detail::peer_state<detail::mouse_state>(*focus_root)) {
            if (focused || focus_root == this) mouse->active = focused;
        }
        if (focused) {
            wnd *root = this;
            while (root->get_parent()) root = root->get_parent();
            if (wnd *old = detail::input_target(*root);
                old && old != this && root != this) {
                const bool tracked = static_cast<bool>(_lifetime);
                const auto weak = detail::mouse_access::lifetime(*this);
                old->on_native_focus(false);
                const auto lifetime = weak.lock();
                if (tracked && (!lifetime || !lifetime->alive)) return;
            }
        }
        if (auto *state = detail::peer_state<detail::input_state>(*this))
            state->focused = focused;
        if (!focused) {
            const bool tracked = static_cast<bool>(_lifetime);
            const auto weak = detail::mouse_access::lifetime(*this);
            detail::mouse_access::cancel_tree(*this, mouse_cancel_reason::focus_lost);
            const auto lifetime = weak.lock();
            if (!tracked || (lifetime && lifetime->alive)) detail::reset_input(*this);
        }
    }

    bool wnd::on_native_key(key_event event) {
        auto *state = detail::peer_state<detail::input_state>(*this);
        const auto key = static_cast<unsigned>(event.key);
        if (!state || !get_input_enabled() ||
            key == 0 || key >= static_cast<unsigned>(key_code::count))
            return false;
        state->modifiers = event.modifiers;
        if (event.action == key_action::release) {
            if (!state->held.test(key)) return false;
            state->held.reset(key);
            event.repeat = false;
        } else {
            if (event.repeat && !state->held.test(key)) return false;
            event.repeat = state->held.test(key);
            state->held.set(key);
        }
        return on_key.emit_consumed(event);
    }

    void wnd::on_native_key_reset() {
        auto *state = detail::peer_state<detail::input_state>(*this);
        if (!state) return;
        state->modifiers = key_modifiers::none;
        if (state->held.none()) return;
        state->held.reset();
        on_key_reset.emit();
    }

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
        if (event.action == mouse_action::press &&
            !get_client_bounds().contains(event.position) &&
            detail::pointer_capture(*this) != this) return;
        if (auto *state = detail::peer_state<detail::mouse_state>(*this)) {
            const unsigned button = unsigned(event.button);
            if (button > 0 && button <= 5) {
                const unsigned mask = 1u << (button - 1);
                state->buttons = static_cast<mouse_buttons>(
                    event.action == mouse_action::press
                        ? unsigned(state->buttons) | mask
                        : unsigned(state->buttons) & ~mask);
            }
        }
        on_mouse_click.emit(event);
    }

    void wnd::on_native_mouse_wheel(mouse_wheel_event event) {
        on_mouse_wheel.emit(event);
    }

    wnd &wnd::set_layout(std::unique_ptr<layout_manager> layout) {
        _layout = std::move(layout);
        if (_layout) {
            for (wnd *child : _children)
                if (!child->_non_client_surface)
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

        const scoped_flag pass(_layout_suspended, *this);
        _layout->relayout(this, get_client_bounds());
    }

    void wnd::attach_non_client(non_client *element) {
        if (!element || std::find(_non_client.begin(),
                                  _non_client.end(), element) !=
                            _non_client.end())
            return;
        _non_client.push_back(element);
        update_non_client(false);
    }

    void wnd::detach_non_client(non_client *element) {
        const auto found = std::find(_non_client.begin(),
                                     _non_client.end(), element);
        if (found == _non_client.end())
            return;
        _non_client.erase(found);
        update_non_client(false);
    }

    void wnd::update_non_client(bool notify_bounds) {
        _non_client_dirty = true;
        if (_updating_non_client) return;
        const detail::window_guard guard(*this);
        const scoped_flag pass(_updating_non_client, *this);
        do {
            _non_client_dirty = false;
            const auto elements = _non_client;
            for (non_client *element : elements) {
                if (element && std::find(_non_client.begin(), _non_client.end(),
                                         element) != _non_client.end())
                    element->on_configuration_changed();
                if (!guard.alive()) return;
            }
        } while (_non_client_dirty);
        if (_created && notify_bounds) on_bounds_changed();
        if (!guard.alive()) return;
        relayout_children();
        if (!guard.alive()) return;
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
        case window_edge::top: {
            const bool spans_window = dynamic_cast<const toolbar *>(target) != nullptr;
            return rect(static_cast<coord>(origin_x + (spans_window ? 0 : left_total)),
                        static_cast<coord>(origin_y+before),
                        static_cast<dim>(spans_window ? width : std::max(0, width-left_total-right_total)),
                        static_cast<dim>(target->_extent));
        }
        case window_edge::right:
            return rect(static_cast<coord>(origin_x+width-before-target->_extent),
                        static_cast<coord>(origin_y+top_total),
                        static_cast<dim>(target->_extent),
                        static_cast<dim>(std::max(0, height-top_total-bottom_total)));
        case window_edge::bottom:
        {
            const bool spans_window =
                dynamic_cast<const status_bar *>(target) != nullptr ||
                dynamic_cast<const toolbar *>(target) != nullptr;
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
        const detail::window_guard owner(*this);
        const auto children = _children;
        std::vector<detail::window_guard> guards;
        guards.reserve(children.size());
        for (auto *child : children) guards.emplace_back(*child);
        for (std::size_t i = 0; i < children.size(); ++i) {
            if (!owner.object_alive()) return;
            if (guards[i].alive() && children[i]->_created)
                children[i]->destroy();
        }
    }

} // namespace native
