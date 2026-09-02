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
#include <utility>

namespace native
{
    // Dispatches an event to connected boolean-returning callbacks.
    template <typename... argument_types> class signal
    {
    public:
        // Construct an empty signal without lazy initialization.
        signal()
            : current_id(0)
            , initialized(false)
            , initializer(nullptr) {}

        //
        // Construct a signal with a one-time lazy initializer.
        //
        // Parameters:
        //      init        - Called before the first connect or emit.
        //
        explicit signal(std::function<void()> init)
            : current_id(0)
            , initialized(false)
            , initializer(std::move(init)) {}

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
            slots[++current_id] = slot;
            return current_id;
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

        // Disconnect the slot with a specific connection identifier.
        void disconnect(int id) const {
            slots.erase(id);
        }

        // Disconnect every slot from this signal.
        void disconnect_all() const {
            slots.clear();
        }

        //
        // Notify slots in reverse connection order.
        //
        // Notes:
        //      Propagation stops when a slot returns true.
        //
        void emit(argument_types... args) {
            ensure_init();
            for (auto slot = slots.rbegin(); slot != slots.rend();
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
            if (!initialized && initializer) {
                initialized = true;
                initializer();
            }
        }

        mutable std::map<int, std::function<bool(argument_types...)>>
            slots;
        mutable int current_id;
        mutable bool initialized;
        std::function<void()> initializer;
    };
} // namespace native
