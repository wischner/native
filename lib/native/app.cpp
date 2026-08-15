//
// Implements backend-neutral application startup state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app.h>
#include <native/screen.h>
#include <native/wnd.h>

namespace native
{
    int app::argc = 0;
    char **app::argv = nullptr;
    char **app::envp = nullptr;
    app_wnd *app::_main_wnd = nullptr;

    int app::run(const app_wnd &wnd) {
        // Populate the backend's screen list before creating a window.
        screen::detect();

        wnd.create();

        // The caller owns the main window for the duration of the loop.
        _main_wnd = const_cast<app_wnd *>(&wnd);

        wnd.show();

        return app::main_loop();
    }

    app_wnd *app::main_wnd() {
        return _main_wnd;
    }
}
