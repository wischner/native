//
// Declares application startup state and the user program entry point.
// Platform launchers populate arguments before entering the event loop.
//
// Also declares app::post, the one operation in this library that may
// be called from a thread other than the one running the event loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <functional>

namespace native
{
    class app_wnd;

    // Coordinates application startup and the platform event loop.
    class app final
    {
    public:
        // Applications use only static app operations.
        app() = delete;

        //
        // Create, show, and run a main application window.
        //
        // Returns:
        //      The platform event loop's process exit code.
        //
        static int run(app_wnd &window);

        //
        // Enter the backend event loop.
        //
        // Returns:
        //      The platform event loop's process exit code.
        //
        static int main_loop();

        //
        // Queue work to run on the user interface thread.
        //
        // Parameters:
        //      work        - Callable invoked once, on the thread
        //                    running the event loop. Ignored when
        //                    empty.
        //
        // Notes:
        //      Safe to call from any thread, and the only safe way for
        //      a worker thread to reach a window. Signals are
        //      synchronous, so emitting one from a worker would run
        //      its slots on that worker thread, touching the backend
        //      from somewhere it does not expect.
        //
        //      Work queued before the loop starts runs once it does.
        //      Work still queued when the loop ends is discarded
        //      rather than run against windows that have gone.
        //
        //      The callable must be copy constructible, which is what
        //      std::function requires. Capture a copyable payload,
        //      such as an owned buffer, rather than an img.
        //
        // Sample call:
        //      std::thread([] {
        //          std::vector<std::uint8_t> frame = receive();
        //          app::post([frame = std::move(frame)] {
        //              // Runs on the UI thread. Safe to repaint.
        //          });
        //      }).detach();
        //
        static void post(std::function<void()> work);

        // Return the current main window, or null before run().
        static app_wnd *main_wnd();

        // Arguments supplied by the platform entry point.
        static int argc;
        static char **argv;
        static char **envp;

    private:
        static app_wnd *_main_wnd;
    };
} // namespace native

//
// Run the application-defined entry point.
//
// Parameters:
//      argc        - Number of command-line arguments.
//      argv        - Null-terminated command-line argument array.
//
// Returns:
//      Process exit code returned to the operating system.
//
extern int program(int argc, char **argv);
