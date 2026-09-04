//
// Declares a move-only scoped signal connection. The handle disconnects
// its callback at scope exit without owning the signal itself.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <functional>

namespace native
{
    template <typename... argument_types> class signal;

    // Disconnects one signal callback when the handle is destroyed.
    class connection final
    {
    public:
        // Construct an empty connection.
        connection() noexcept = default;

        // Transfer a scoped connection from another handle.
        connection(connection &&source) noexcept;

        // Disconnect the current callback and take another handle.
        connection &operator=(connection &&source) noexcept;

        connection(const connection &) = delete;
        connection &operator=(const connection &) = delete;

        // Disconnect the callback when this handle still owns it.
        ~connection();

        // Relinquish ownership while leaving the callback connected.
        void release() noexcept;

        // Return whether this handle still owns a scoped connection.
        explicit operator bool() const noexcept;

    private:
        template <typename... argument_types> friend class signal;

        explicit connection(std::function<void()> disconnect);

        std::function<void()> _disconnect;
    };
} // namespace native
