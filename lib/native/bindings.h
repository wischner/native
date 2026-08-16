//
// Implements an internal bidirectional backend handle registry.
// Template bodies stay here because every backend selects distinct
// handle and object types at compile time.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <unordered_map>

namespace native
{

    // Maps backend handles to objects in both directions.
    template <typename handle_type, typename object_type> class bindings
    {
    public:
        // Register or replace a handle/object association.
        void register_pair(const handle_type &handle,
                           const object_type &object) {
            // Remove either previous side so the two maps cannot retain
            // stale associations when a handle or object is reused.
            unregister_by_handle(handle);
            unregister_by_object(object);
            handle_to_object_[handle] = object;
            object_to_handle_[object] = handle;
        }

        // Remove the association identified by a backend handle.
        void unregister_by_handle(const handle_type &handle) {
            auto item = handle_to_object_.find(handle);
            if (item != handle_to_object_.end()) {
                object_to_handle_.erase(item->second);
                handle_to_object_.erase(item);
            }
        }

        // Remove the association identified by its object.
        void unregister_by_object(const object_type &object) {
            auto item = object_to_handle_.find(object);
            if (item != object_to_handle_.end()) {
                handle_to_object_.erase(item->second);
                object_to_handle_.erase(item);
            }
        }

        // Return the object for a handle, or a default value if absent.
        object_type
        object_from_handle(const handle_type &handle) const {
            auto item = handle_to_object_.find(handle);
            return item != handle_to_object_.end() ? item->second
                                                   : object_type{};
        }

        // Return the object's handle, or a default value if absent.
        handle_type
        handle_from_object(const object_type &object) const {
            auto item = object_to_handle_.find(object);
            return item != object_to_handle_.end() ? item->second
                                                   : handle_type{};
        }

        // Remove every association from both lookup directions.
        void clear() {
            handle_to_object_.clear();
            object_to_handle_.clear();
        }

    private:
        std::unordered_map<handle_type, object_type> handle_to_object_;
        std::unordered_map<object_type, handle_type> object_to_handle_;
    };

} // namespace native
