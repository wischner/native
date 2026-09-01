//
// Declares the private hand-off between worker threads and the event
// loop.
//
// app::post queues work from any thread. A backend's event loop drains
// that queue on the user interface thread, once per iteration. A
// backend whose loop blocks must also install a wake routine, or
// queued work waits for the next input event to arrive.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

namespace native::detail
{
    //
    // Run every callable queued by app::post since the last call.
    //
    // Notes:
    //      Call from the event loop and from nowhere else. Backends
    //      should call it once per iteration, after dispatching
    //      input and before presenting, so posted work and the
    //      repaint it asks for land in the same pass.
    //
    //      Cheap when the queue is empty, which is the usual case, so
    //      calling it from a fast polling loop costs nothing.
    //
    void drain_posted_work();

    //
    // Install the routine that wakes a blocked event loop.
    //
    // Parameters:
    //      wake        - Backend routine called after work is queued,
    //                    from the posting thread. Must be safe to
    //                    call from any thread. Pass nullptr to
    //                    remove it.
    //
    // Notes:
    //      Only a backend whose loop blocks needs this. A loop that
    //      polls, such as the SDL2 backend's, already comes back
    //      around on its own and may leave it unset.
    //
    //      Called with no library lock held, so a backend is free to
    //      take its own.
    //
    void set_loop_wake(void (*wake)());

    //
    // Discard queued work without running it.
    //
    // Notes:
    //      Used when the event loop ends. Work posted for a window
    //      that no longer exists must not run.
    //
    void discard_posted_work();
} // namespace native::detail
