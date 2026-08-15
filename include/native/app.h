//
// Declares application startup state and the user program entry point.
// Platform launchers populate arguments before entering the event loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

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
        static int run(const app_wnd &window);

        //
        // Enter the backend event loop.
        //
        // Returns:
        //      The platform event loop's process exit code.
        //
        static int main_loop();

        // Return the current main window, or null before run().
        static app_wnd *main_wnd();

        // Arguments supplied by the platform entry point.
        static int argc;
        static char **argv;
        static char **envp;

    private:
        static app_wnd *_main_wnd;
    };
}

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
