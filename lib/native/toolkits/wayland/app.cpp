//
// Runs GTK main-context events and bounded portable worker delivery.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../post_backend.h"
namespace
{
    void wake() {
        g_main_context_wakeup(g_main_context_default());
    }
} // namespace
namespace native
{
    int app::main_loop() {
        linux::wayland::exit_requested = false;
        detail::set_loop_wake(wake);
        detail::drain_posted_work();
        while (!linux::wayland::exit_requested) {
            g_main_context_iteration(nullptr, true);
            detail::drain_posted_work();
        }
        detail::set_loop_wake(nullptr);
        return 0;
    }
} // namespace native
