//
// Implements the public signal-and-slot utility used by native events.
// Template definitions remain in this header because each subscriber's
// callable signature is selected by application code at compile time.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "connection.h"

namespace native
{
    // Dispatches an event to connected boolean-returning callbacks.
    template <typename... argument_types> class signal
    {
        using slot_type =
            std::function<bool(argument_types...)>;

        struct state
        {
            std::map<int, slot_type> slots;
            int current_id = 0;
            bool initialized = false;
            std::function<void()> initializer;
        };

    public:
        // Construct an empty signal without lazy initialization.
        signal()
            : _state(std::make_shared<state>()) {}

        //
        // Construct a signal with a one-time lazy initializer.
        //
        // Parameters:
        //      init        - Called before the first connect or emit.
        //
        explicit signal(std::function<void()> init)
            : _state(std::make_shared<state>()) {
            _state->initializer = std::move(init);
        }

        // Copy callbacks into an independent signal state.
        signal(const signal &source)
            : _state(std::make_shared<state>(*source._state)) {}

        // Replace callbacks with an independent copy.
        signal &operator=(const signal &source) {
            if (this != &source)
                _state = std::make_shared<state>(*source._state);
            return *this;
        }

        signal(signal &&) noexcept = default;
        signal &operator=(signal &&) noexcept = default;

        //
        // Connect a callable slot.
        //
        // Parameters:
        //      slot        - Callback; true stops propagation.
        //
        // Returns:
        //      Connection identifier accepted by disconnect().
        //
        int connect(
            const std::function<bool(argument_types...)> &slot) const {
            ensure_init();
            _state->slots[++_state->current_id] = slot;
            return _state->current_id;
        }

        // Connect a callable and return an automatic lifetime handle.
        [[nodiscard]] connection connect_scoped(
            const slot_type &slot) const {
            const int id = connect(slot);
            std::weak_ptr<state> weak_state = _state;
            return connection([weak_state, id]() {
                if (auto connected = weak_state.lock())
                    connected->slots.erase(id);
            });
        }

        //
        // Connect a non-const member function to an object.
        //
        // Returns:
        //      Connection identifier accepted by disconnect().
        //
        template <typename object_type>
        int connect(object_type *instance,
                    bool (object_type::*method)(argument_types...)) {
            return connect([=](argument_types... args) {
                return (instance->*method)(
                    std::forward<argument_types>(args)...);
            });
        }

        // Connect a member function with automatic disconnection.
        template <typename object_type>
        [[nodiscard]] connection connect_scoped(
            object_type *instance,
            bool (object_type::*method)(argument_types...)) {
            return connect_scoped([=](argument_types... args) {
                return (instance->*method)(
                    std::forward<argument_types>(args)...);
            });
        }

        //
        // Connect a const member function to an object.
        //
        // Returns:
        //      Connection identifier accepted by disconnect().
        //
        template <typename object_type>
        int connect(const object_type *instance,
                    bool (object_type::*method)(argument_types...)
                        const) {
            return connect([=](argument_types... args) {
                return (instance->*method)(
                    std::forward<argument_types>(args)...);
            });
        }

        // Connect a const member function with automatic disconnection.
        template <typename object_type>
        [[nodiscard]] connection connect_scoped(
            const object_type *instance,
            bool (object_type::*method)(argument_types...) const) {
            return connect_scoped([=](argument_types... args) {
                return (instance->*method)(
                    std::forward<argument_types>(args)...);
            });
        }

        // Disconnect the slot with a specific connection identifier.
        void disconnect(int id) const {
            _state->slots.erase(id);
        }

        // Disconnect every slot from this signal.
        void disconnect_all() const {
            _state->slots.clear();
        }

        //
        // Notify slots in reverse connection order.
        //
        // Notes:
        //      Propagation stops when a slot returns true.
        //
        void emit(argument_types... args) {
            ensure_init();
            for (auto slot = _state->slots.rbegin();
                 slot != _state->slots.rend();
                 ++slot) {
                // Named arguments are lvalues here. Passing them
                // without forwarding gives every by-value subscriber
                // its own copy while preserving reference arguments.
                if (slot->second(args...))
                    break;
            }
        }

    private:
        // Run the optional initializer at most once.
        void ensure_init() const {
            if (!_state->initialized && _state->initializer) {
                _state->initialized = true;
                _state->initializer();
            }
        }

        std::shared_ptr<state> _state;
    };
} // namespace native
