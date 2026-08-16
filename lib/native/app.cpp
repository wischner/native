//
// Implements backend-neutral application startup state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app.h>
#include <native/app_wnd.h>
#include <native/screen.h>
#include <native/wnd.h>

#include <stdexcept>

namespace native
{
    int app::argc = 0;
    char **app::argv = nullptr;
    char **app::envp = nullptr;
    app_wnd *app::_main_wnd = nullptr;

    int app::run(const app_wnd &wnd) {
        if (_main_wnd)
            throw std::logic_error(
                "An application event loop is already active.");
        if (wnd.get_owner())
            throw std::invalid_argument(
                "An owned window cannot be the application main "
                "window.");

        _main_wnd = const_cast<app_wnd *>(&wnd);

        try {
            // Populate screens before creation so handlers may query
            // them from the window's create signal.
            screen::detect();
            wnd.create();
            wnd.show();

            const int result = app::main_loop();
            wnd.destroy();
            _main_wnd = nullptr;
            return result;
        } catch (...) {
            wnd.destroy();
            _main_wnd = nullptr;
            throw;
        }
    }

    app_wnd *app::main_wnd() {
        return _main_wnd;
    }
} // namespace native
