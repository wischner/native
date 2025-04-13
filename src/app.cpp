#include <native.h>

namespace native
{
    int app::run(const app_wnd &wnd)
    {
        // Detect screen.
        screen::detect();

        // Create and show window (main window).
        wnd.create();

        // Store it to app main window.
        _main_wnd = const_cast<app_wnd *>(&wnd);

        // And show it.
        wnd.show();

        // And enter main loop.
        return app::main_loop();
    }

    app_wnd *app::main_wnd()
    {
        return _main_wnd;
    }
}