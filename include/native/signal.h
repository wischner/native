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
#include <limits>
#include <stdexcept>
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
            bool alive = true;
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

        // Stop an in-flight emission before any further owner callbacks.
        ~signal() { if (_state) _state->alive = false; }

        // Transfer callbacks and existing connection handles.
        signal(signal &&) noexcept = default;
        // Replace this signal with transferred callback ownership.
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
            const auto state = ensure_init();
            if (!state->alive) return 0;
            if (state->current_id == std::numeric_limits<int>::max())
                throw std::overflow_error("Signal connection identifiers exhausted.");
            const int id = ++state->current_id;
            state->slots.emplace(id, slot);
            return id;
        }

        // Connect a callable and return an automatic lifetime handle.
        [[nodiscard]] connection connect_scoped(
            const slot_type &slot) const {
            std::weak_ptr<state> weak_state = _state;
            const int id = connect(slot);
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
            (void)emit_consumed(args...);
        }

        // Notify newest-first and report whether a callback consumed input.
        bool emit_consumed(argument_types... args) {
            const auto keep_alive = ensure_init();
            auto next = keep_alive->slots.end();
            // Re-find by ID after each callback: scoped disconnection can
            // erase the current or a later slot, including during deletion.
            int before = 0;
            bool first = true;
            while (keep_alive->alive) {
                next = first ? keep_alive->slots.end()
                             : keep_alive->slots.lower_bound(before);
                first = false;
                if (next == keep_alive->slots.begin()) break;
                --next;
                before = next->first;
                const auto callback = next->second;
                if (callback(args...)) return true;
            }
            return false;
        }

    private:
        // Run the optional initializer at most once.
        std::shared_ptr<state> ensure_init() const {
            const auto keep_alive = _state;
            if (!keep_alive->initialized && keep_alive->initializer) {
                keep_alive->initialized = true;
                keep_alive->initializer();
            }
            return keep_alive;
        }

        std::shared_ptr<state> _state;
    };
} // namespace native
