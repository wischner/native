//
// Declares portable pointer capabilities and scoped capture leases.
// Tokens borrow one native resource generation and are UI-thread objects.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <memory>

namespace native
{
    class wnd;
    namespace detail { struct mouse_capture_state; }

    enum class mouse_capture_mode { drag, relative };
    struct mouse_capture_options
    {
        mouse_capture_mode mode = mouse_capture_mode::drag;
        bool require_unaccelerated = false;
    };
    struct mouse_capabilities
    {
        bool hidden_cursor = false;
        bool drag_capture = false;
        bool relative_motion = false;
        bool unaccelerated_motion = false;
    };

    // Owns a single capture lease; stale leases cannot affect a new owner.
    class mouse_capture
    {
    public:
        // Construct an inactive token.
        mouse_capture();
        // Release an active lease on the UI thread.
        ~mouse_capture();
        // Transfer the lease without releasing it.
        mouse_capture(mouse_capture &&other) noexcept;
        // Release this lease and take the other lease.
        mouse_capture &operator=(mouse_capture &&other) noexcept;
        // Capture ownership cannot be copied.
        mouse_capture(const mouse_capture &) = delete;
        // Capture ownership cannot be copy-assigned.
        mouse_capture &operator=(const mouse_capture &) = delete;
        // Return whether the resource still owns this lease.
        bool get_active() const;
        // Release normally without emitting cancellation; idempotent.
        void release();
    private:
        friend class wnd;
        explicit mouse_capture(std::shared_ptr<detail::mouse_capture_state> state);
        std::shared_ptr<detail::mouse_capture_state> _state;
    };
}
