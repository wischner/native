//
// Implements the common backend-resource peer adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "wnd_peer.h"
#include "input_state.h"

#include <utility>
#include <stdexcept>

#include <native/wnd.h>

namespace native::detail
{
    window_guard::window_guard(wnd &owner)
        : _object(wnd_peer_access::object_lifetime(owner))
        , _resource(wnd_peer_access::resource_lifetime(owner))
        , _tracked(owner.get_created()) {}

    bool window_guard::object_alive() const {
        const auto object = _object.lock();
        return object && object->alive;
    }
    bool window_guard::alive() const {
        const auto resource = _resource.lock();
        return object_alive() && (!_tracked || (resource && resource->alive));
    }
    std::weak_ptr<wnd_lifetime> wnd_peer_access::object_lifetime(wnd &owner) {
        return owner._object_lifetime;
    }
    std::weak_ptr<wnd_lifetime> wnd_peer_access::resource_lifetime(wnd &owner) {
        return owner._lifetime;
    }

    // Adapts the established backend hooks to the narrow peer contract.
    class backend_wnd_peer final : public wnd_peer
    {
    public:
        explicit backend_wnd_peer(wnd &owner)
            : _owner(owner) {}

        void apply_bounds(const rect &) override {
            _owner.apply_bounds();
        }

        void apply_visible(bool visible) override {
            if (visible)
                _owner.show_native();
        }

        void invalidate(const rect &area) override {
            _owner.invalidate_native(area);
        }

    private:
        wnd &_owner;
    };

    wnd_peer::~wnd_peer() = default;

    wnd_peer::owned_state::owned_state(
        void *value, void (*destroy)(void *))
        : value(value)
        , destroy(destroy) {}

    wnd_peer::owned_state::owned_state(owned_state &&other) noexcept
        : value(other.release())
        , destroy(other.destroy) {}

    wnd_peer::owned_state &wnd_peer::owned_state::operator=(
        owned_state &&other) noexcept {
        if (this == &other)
            return *this;
        if (value && destroy)
            destroy(value);
        destroy = other.destroy;
        value = other.release();
        return *this;
    }

    wnd_peer::owned_state::~owned_state() {
        if (value && destroy)
            destroy(value);
    }

    void *wnd_peer::owned_state::release() noexcept {
        void *released = value;
        value = nullptr;
        return released;
    }

    wnd_peer *wnd_peer::content_peer(const wnd &) {
        return this;
    }

    void wnd_peer_access::assign_state(
        wnd &owner,
        std::type_index type,
        void *state,
        void (*destroy)(void *)) {
        wnd_peer::owned_state acquired(state, destroy);
        if (!owner._peer)
            throw std::logic_error("Cannot assign state to an uncreated window.");
        owner._peer->_states.insert_or_assign(type, std::move(acquired));
    }

    void *wnd_peer_access::state(
        const wnd &owner, std::type_index type) {
        if (!owner._peer)
            return nullptr;
        const auto found = owner._peer->_states.find(type);
        return found == owner._peer->_states.end()
                   ? nullptr
                   : found->second.value;
    }

    void *wnd_peer_access::release_state(
        wnd &owner, std::type_index type) {
        if (!owner._peer)
            return nullptr;
        const auto found = owner._peer->_states.find(type);
        if (found == owner._peer->_states.end())
            return nullptr;
        void *state = found->second.release();
        owner._peer->_states.erase(found);
        return state;
    }

    void wnd_peer_access::assign_content(wnd &owner, void *content) {
        if (owner._peer)
            owner._peer->_content = content;
    }

    void *wnd_peer_access::content(
        const wnd &owner, const wnd *child) {
        if (!owner._peer)
            return nullptr;
        wnd_peer *content = owner._peer->content_peer(
            child ? *child : owner);
        return content ? content->_content : nullptr;
    }

    std::unique_ptr<wnd_peer> create_wnd_peer(wnd &owner) {
        return std::make_unique<backend_wnd_peer>(owner);
    }
} // namespace native::detail
