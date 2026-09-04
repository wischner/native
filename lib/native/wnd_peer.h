//
// Declares the narrow backend-resource peer owned by every created window.
// The public header sees only its forward declaration.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>

#include <native/geometry.h>

namespace native
{
    class wnd;

    namespace detail
    {
        // Routes common resource operations without inspecting control type.
        class wnd_peer
        {
        public:
            virtual ~wnd_peer();

            // Return the peer into which one child should be parented.
            virtual wnd_peer *content_peer(const wnd &child);

            // Apply complete cached geometry to the backend resource.
            virtual void apply_bounds(const rect &bounds) = 0;

            // Apply resource visibility.
            virtual void apply_visible(bool visible) = 0;

            // Schedule repaint of one client rectangle.
            virtual void invalidate(const rect &area) = 0;

        private:
            struct owned_state
            {
                void *value = nullptr;
                void (*destroy)(void *) = nullptr;

                owned_state() = default;
                owned_state(void *value, void (*destroy)(void *));
                owned_state(owned_state &&other) noexcept;
                owned_state &operator=(owned_state &&other) noexcept;
                ~owned_state();

                owned_state(const owned_state &) = delete;
                owned_state &operator=(const owned_state &) = delete;

                void *release() noexcept;
            };

            std::unordered_map<std::type_index, owned_state> _states;
            void *_content = nullptr;

            friend class wnd_peer_access;
        };

        // Gives internal backend adapters typed access to peer-owned state.
        class wnd_peer_access
        {
        public:
            static void assign_state(wnd &owner,
                                     std::type_index type,
                                     void *state,
                                     void (*destroy)(void *));
            static void *state(const wnd &owner, std::type_index type);
            static void *release_state(wnd &owner, std::type_index type);
            static void assign_content(wnd &owner, void *content);
            static void *content(const wnd &owner,
                                 const wnd *child = nullptr);
        };

        // Default for backend state without a child-content resource.
        inline void *peer_content(...) {
            return nullptr;
        }

        // Store one backend state object in its window's peer.
        template <typename state_type>
        void assign_peer_state(wnd &owner, state_type *state) {
            wnd_peer_access::assign_state(
                owner,
                typeid(state_type),
                state,
                [](void *value) {
                    delete static_cast<state_type *>(value);
                });
        }

        // Return peer state by its backend type, or null when absent.
        template <typename state_type>
        state_type *peer_state(const wnd &owner) {
            return static_cast<state_type *>(wnd_peer_access::state(
                owner, typeid(state_type)));
        }

        // Adapts former object-to-state bindings to direct peer storage.
        template <typename handle_type, typename object_type>
        class peer_bindings
        {
            static_assert(std::is_pointer_v<handle_type>);
            static_assert(std::is_pointer_v<object_type>);
            using state_type = std::remove_pointer_t<object_type>;

        public:
            void register_pair(const handle_type &handle,
                               const object_type &object) const {
                if (handle) {
                    assign_peer_state(*handle, object);
                    if (void *content = peer_content(object))
                        wnd_peer_access::assign_content(*handle, content);
                }
            }

            void unregister_by_handle(const handle_type &handle) const {
                if (handle) {
                    wnd_peer_access::release_state(
                        *handle, typeid(state_type));
                }
            }

            object_type object_from_handle(
                const handle_type &handle) const {
                return handle ? peer_state<state_type>(*handle) : nullptr;
            }

            // Peer state follows its owner, so there is no global map to clear.
            void clear() const noexcept {}
        };

        // Construct the compatibility peer around existing backend hooks.
        std::unique_ptr<wnd_peer> create_wnd_peer(wnd &owner);
    } // namespace detail
} // namespace native
