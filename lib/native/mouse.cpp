//
// Implements generation-safe pointer hover, rich motion and capture leases.
// One application pointer owns one hover target and one exclusive lease.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "mouse_state.h"
#include "emulated_tree.h"
#include <native/canvas.h>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{
    native::wnd *hover_owner = nullptr;
    std::weak_ptr<native::detail::wnd_lifetime> hover_lifetime;
    std::weak_ptr<native::detail::mouse_capture_state> capture;
    bool changing_hover = false;

    bool ancestors_contain(native::wnd &owner, native::point local) {
        for (native::wnd *child = &owner; child->get_parent(); child = child->get_parent()) {
            const auto position = child->get_position();
            local = native::point(static_cast<native::coord>(int(local.x) + position.x),
                                  static_cast<native::coord>(int(local.y) + position.y));
            auto *parent = child->get_parent();
            if (!parent->get_visible() || !parent->get_client_bounds().contains(local))
                return false;
        }
        return true;
    }

    bool live(const std::weak_ptr<native::detail::wnd_lifetime> &weak) {
        auto token = weak.lock();
        return token && token->alive;
    }
}
namespace native::detail
{
    std::weak_ptr<wnd_lifetime> mouse_access::lifetime(wnd &owner) {
        return owner._lifetime;
    }

    void mouse_access::refresh(wnd &owner) {
        if (owner._created) owner.apply_cursor();
    }

    void mouse_access::cancel_tree(wnd &root, mouse_cancel_reason reason,
                                  bool notify) {
        const auto lifetime = root._lifetime;
        std::vector<std::pair<wnd *, std::weak_ptr<wnd_lifetime>>> children;
        for (wnd *child : root._children)
            if (child) children.emplace_back(child, child->_lifetime);
        pointer_cancel(root, reason, notify);
        for (const auto &[child, weak] : children)
            if (live(weak)) cancel_tree(*child, reason, notify);
        (void)lifetime;
    }

    void release_pointer(const std::shared_ptr<mouse_capture_state> &state) {
        if (!state || !state->active) return;
        state->active = false;
        if (capture.lock() == state) capture.reset();
        // The resource can be marked dead before its native teardown begins.
        if (state->owner && !state->lifetime.expired()) {
            if (auto *mouse = peer_state<mouse_state>(*state->owner))
                mouse->baseline = false;
            backend_release_mouse(*state->owner, state->options.mode);
        }
    }

    wnd *pointer_capture(wnd &root) {
        auto lease = capture.lock();
        return lease && lease->active && live(lease->lifetime) &&
            root_of(lease->owner) == root_of(&root) ? lease->owner : nullptr;
    }

    bool pointer_relative(wnd &root) {
        auto lease = capture.lock();
        return pointer_capture(root) && lease &&
            lease->options.mode == mouse_capture_mode::relative;
    }

    void pointer_cancel(wnd &owner, mouse_cancel_reason reason, bool notify) {
        const bool previous_transition = changing_hover;
        changing_hover = true;
        struct restore_transition {
            bool previous;
            ~restore_transition() { changing_hover = previous; }
        } transition{previous_transition};
        auto *state = peer_state<mouse_state>(owner);
        const auto weak = mouse_access::lifetime(owner);
        bool changed = state && (state->inside ||
            state->buttons != mouse_buttons::none);
        if (hover_owner == &owner) {
            hover_owner = nullptr;
            hover_lifetime.reset();
        }
        if (state) {
            state->inside = false;
            state->baseline = false;
            state->buttons = mouse_buttons::none;
            state->suspended = true;
        }
        if (auto lease = capture.lock(); lease && lease->owner == &owner) {
            changed = changed || lease->active;
            release_pointer(lease);
        }
        if (reason != mouse_cancel_reason::destroyed) backend_refresh_mouse(owner);
        if (notify && live(weak)) {
            if (auto *current = peer_state<mouse_state>(owner)) current->cancel_pending |= changed;
            owner.on_native_mouse_cancel(reason);
        }
    }

    mouse_cursor pointer_cursor(wnd &owner, point local) {
        const auto *root = peer_state<mouse_state>(*root_of(&owner));
        const auto *state = peer_state<mouse_state>(owner);
        if (!owner.get_visible() || !owner.get_input_enabled() ||
            (root && !root->active) || (state && state->suspended) ||
            !ancestors_contain(owner, local)) return mouse_cursor::arrow;
        const auto shape = owner.get_cursor_at(local);
        return shape == mouse_cursor::hidden && !owner.get_client_bounds().contains(local)
            ? mouse_cursor::arrow : shape;
    }

    void pointer_hover(wnd *target, point local) {
        if (changing_hover) return;
        const auto next_lifetime = target ? mouse_access::lifetime(*target)
                                         : std::weak_ptr<wnd_lifetime>{};
        const auto *root_state = target ? peer_state<mouse_state>(*root_of(target)) : nullptr;
        if (target && (!live(next_lifetime) || (root_state && !root_state->active) || !target->get_visible() ||
            !target->get_input_enabled() ||
            !target->get_client_bounds().contains(local) ||
            !ancestors_contain(*target, local))) target = nullptr;
        if (!live(hover_lifetime)) hover_owner = nullptr;
        if (hover_owner == target) return;
        changing_hover = true;
        struct reset_flag { ~reset_flag() { changing_hover = false; } } guard;
        wnd *old = std::exchange(hover_owner, nullptr);
        const auto old_lifetime = hover_lifetime;
        hover_lifetime.reset();
        if (old && live(old_lifetime)) {
            auto *state = peer_state<mouse_state>(*old);
            const point last = state ? state->position : point{};
            old->on_native_mouse_leave(last);
        }
        // Leave/cancel callbacks may destroy, hide or replace the next target.
        if (!target || !live(next_lifetime) || !target->get_visible() ||
            !target->get_input_enabled() ||
            !target->get_client_bounds().contains(local) ||
            !ancestors_contain(*target, local)) return;
        if (auto *state = peer_state<mouse_state>(*target)) state->suspended = false;
        hover_owner = target;
        hover_lifetime = next_lifetime;
        target->on_native_mouse_enter(local);
    }

    void pointer_leave(wnd &owner, point local) {
        if (hover_owner == &owner) pointer_hover(nullptr, local);
    }

    void pointer_position(wnd &root, point position) {
        if (!root.get_created() || !root.get_visible() ||
            !rect(0, 0, root.get_dimensions().w,
                        root.get_dimensions().h).contains(position)) {
            pointer_hover(nullptr, position);
            return;
        }
        wnd *target = deepest_at(root, position);
        point local = position;
        if (target != &root) {
            const point origin = origin_in_root(*target);
            local = point(static_cast<coord>(position.x - origin.x),
                          static_cast<coord>(position.y - origin.y));
        }
        pointer_hover(target, local);
    }
}
namespace native
{
    mouse_capture::mouse_capture() = default;
    mouse_capture::mouse_capture(
        std::shared_ptr<detail::mouse_capture_state> state)
        : _state(std::move(state)) {}
    mouse_capture::~mouse_capture() { release(); }
    mouse_capture::mouse_capture(mouse_capture &&other) noexcept = default;
    mouse_capture &mouse_capture::operator=(mouse_capture &&other) noexcept {
        if (this != &other) { release(); _state = std::move(other._state); }
        return *this;
    }
    bool mouse_capture::get_active() const {
        return _state && _state->active && live(_state->lifetime);
    }
    void mouse_capture::release() { detail::release_pointer(_state); }

    mouse_capabilities wnd::get_mouse_capabilities() const {
        return detail::backend_mouse_capabilities();
    }
    bool wnd::get_mouse_inside() const {
        auto *state = detail::peer_state<detail::mouse_state>(*this);
        return state && state->inside;
    }
    std::optional<mouse_capture> wnd::capture_mouse(
        mouse_capture_options options, std::string &error) {
        error.clear();
        if (options.mode != mouse_capture_mode::drag &&
            options.mode != mouse_capture_mode::relative)
            throw std::invalid_argument("Invalid mouse capture mode.");
        auto weak = detail::mouse_access::lifetime(*this);
        auto *input = detail::peer_state<detail::input_state>(*this);
        if (!live(weak) || !get_visible() || !get_input_enabled() ||
            !input || !input->focused) {
            error = "Capture requires a created, visible, focused client.";
            return std::nullopt;
        }
        if (auto lease = capture.lock(); lease && lease->active) {
            error = "Another pointer lease is active.";
            return std::nullopt;
        }
        const auto supported = get_mouse_capabilities();
        if ((options.mode == mouse_capture_mode::drag && !supported.drag_capture) ||
            (options.mode == mouse_capture_mode::relative && !supported.relative_motion) ||
            (options.require_unaccelerated && !supported.unaccelerated_motion)) {
            error = "Requested pointer mode is unsupported by this backend.";
            return std::nullopt;
        }
        auto state = std::make_shared<detail::mouse_capture_state>();
        state->owner = this;
        state->lifetime = weak;
        state->options = options;
        if (!detail::backend_capture_mouse(*this, options, error))
            return std::nullopt;
        if (!live(weak)) {
            error = "Window destroyed during capture acquisition.";
            return std::nullopt;
        }
        state->active = true;
        capture = state;
        if (auto *mouse = detail::peer_state<detail::mouse_state>(*this))
            mouse->baseline = false;
        return mouse_capture(std::move(state));
    }
    void wnd::on_native_mouse_enter(const point &position) {
        auto *state = detail::peer_state<detail::mouse_state>(*this);
        if (!state || state->inside) return;
        state->inside = true;
        state->baseline = false;
        state->position = position;
        on_mouse_enter.emit(position);
    }
    void wnd::on_native_mouse_leave(const point &position) {
        auto *state = detail::peer_state<detail::mouse_state>(*this);
        if (!state || !state->inside) return;
        const auto weak = detail::mouse_access::lifetime(*this);
        const bool held = state->buttons != mouse_buttons::none;
        const bool captured = detail::pointer_capture(*this) == this ||
            (dynamic_cast<canvas *>(this) && held);
        state->inside = false;
        state->baseline = false;
        if (!captured) state->buttons = mouse_buttons::none;
        on_mouse_leave.emit(position);
        if (held && !captured && live(weak)) {
            if (auto *current = detail::peer_state<detail::mouse_state>(*this))
                current->cancel_pending = true;
            on_native_mouse_cancel(mouse_cancel_reason::left_client);
        }
    }
    void wnd::on_native_mouse_cancel(mouse_cancel_reason reason) {
        auto *state = detail::peer_state<detail::mouse_state>(*this);
        const bool notify = state && (state->cancel_pending ||
            state->buttons != mouse_buttons::none || state->inside);
        if (state) {
            state->buttons = mouse_buttons::none;
            state->baseline = false;
            state->cancel_pending = false;
        }
        if (notify && _created && !_destroying) on_mouse_cancel.emit(reason);
    }
    void wnd::on_native_mouse_motion(mouse_motion_event event) {
        if (!std::isfinite(event.dx) || !std::isfinite(event.dy))
            throw std::invalid_argument("Mouse motion must be finite.");
        auto *state = detail::peer_state<detail::mouse_state>(*this);
        const auto *root = detail::peer_state<detail::mouse_state>(*detail::root_of(this));
        if (!state || !get_input_enabled() || (root && !root->active)) return;
        event.buttons = state->buttons;
        on_mouse_motion.emit(event);
    }
}
