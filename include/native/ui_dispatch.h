//
// Declares bounded lifetime-scoped worker delivery over the native UI loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <cstddef>
#include <functional>
#include <memory>

namespace native
{
    class wnd;
    namespace detail { struct ui_dispatch_state; }
    enum class ui_post_result { accepted, full, closed };

    // Copyable weak endpoint. Posting never accesses a window on the worker.
    class ui_sender
    {
    public:
        // Construct an inert endpoint; every submission returns closed.
        ui_sender();
        // Admit one FIFO callback; accepted does not guarantee delivery.
        ui_post_result post(std::function<void(wnd &)> work) const;
        // Replace the latest progress callback, keeping one pending wake.
        ui_post_result post_latest(std::function<void(wnd &)> work) const;
    private:
        friend class ui_dispatch_scope;
        std::weak_ptr<detail::ui_dispatch_state> _state;
    };

    // UI-thread-owned receiver scope; invalidate before destroying borrowed data.
    class ui_dispatch_scope
    {
    public:
        // Bind a created receiver and bounded FIFO. Invalid capacity throws.
        explicit ui_dispatch_scope(wnd &receiver, std::size_t capacity = 64);
        // Close the scope; queued work becomes inert, workers need no UI reply.
        ~ui_dispatch_scope();
        ui_dispatch_scope(const ui_dispatch_scope &) = delete;
        ui_dispatch_scope &operator=(const ui_dispatch_scope &) = delete;
        // Return a weak thread-safe submission endpoint.
        ui_sender sender() const;
        // Invalidate on the UI thread; idempotent, does not join workers.
        void close();
    private:
        std::shared_ptr<detail::ui_dispatch_state> _state;
    };
}
