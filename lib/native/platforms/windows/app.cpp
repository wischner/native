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

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return static_cast<int>(msg.wParam);
    }

} // namespace native
