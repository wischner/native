//
// Implements the Windows application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <windows.h>

#include "../../post_backend.h"
#include "globals.h"

namespace native
{

    int app::main_loop() {
        MSG msg;
        BOOL ret;

        detail::drain_posted_work();
        while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
            if (ret == -1) {
                // Handle error if needed
                return -1;
            }

            bool translated = false;
            if (auto *window = app::main_wnd();
                window && window->menu.id()) {
                auto *menu = windows::menu_bindings.object_from_handle(
                    window->menu.id());
                HWND hwnd = windows::wnd_bindings.handle_from_object(window);
                translated = menu && menu->accelerators && hwnd &&
                    TranslateAcceleratorW(hwnd, menu->accelerators, &msg);
            }
            if (translated)
                continue;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return static_cast<int>(msg.wParam);
    }

} // namespace native
