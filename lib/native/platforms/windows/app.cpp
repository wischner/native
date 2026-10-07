//
// Implements the Windows application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <windows.h>
#include <atomic>

#include "../../post_backend.h"
#include "globals.h"
#include "../../input_state.h"

namespace
{
    std::atomic<DWORD> posted_thread = 0;

    // Wake GetMessage even when posted work is the only pending event.
    void wake_posted_work() {
        if (const DWORD thread = posted_thread.load())
            PostThreadMessageW(thread, WM_APP + 0x71, 0, 0);
    }

    struct posted_session
    {
        posted_session() {
            MSG message;
            PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            posted_thread.store(GetCurrentThreadId());
            native::detail::set_loop_wake(wake_posted_work);
        }
        ~posted_session() {
            native::detail::set_loop_wake(nullptr);
            posted_thread.store(0);
        }
    };
}

namespace native
{

    int app::main_loop() {
        const posted_session posted;
        MSG msg;
        BOOL ret;

        detail::drain_posted_work();
        while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
            if (ret == -1) {
                // Handle error if needed
                return -1;
            }

            bool translated = false;
            auto *target = windows::wnd_bindings.object_from_handle(msg.hwnd);
            const bool editor = dynamic_cast<text_edit *>(target) ||
                dynamic_cast<code_edit *>(target) || dynamic_cast<combo_box *>(target);
            const bool paired_key = target &&
                (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
                detail::key_held(*target, detail::pc_key((msg.lParam >> 16) & 0xff,
                    (msg.lParam & (1L << 24)) != 0));
            const bool editing_shortcut = editor && msg.message == WM_KEYDOWN &&
                (GetKeyState(VK_CONTROL) & 0x8000) &&
                (msg.wParam == 'A' || msg.wParam == 'C' || msg.wParam == 'X' ||
                 msg.wParam == 'V' || msg.wParam == 'Z' || msg.wParam == 'Y');
            if (auto *window = app::main_wnd();
                window && window->menu.id()) {
                auto *menu = windows::menu_bindings.object_from_handle(
                    window->menu.id());
                HWND hwnd = windows::wnd_bindings.handle_from_object(window);
                translated = !editing_shortcut && !paired_key && menu && menu->accelerators && hwnd &&
                    TranslateAcceleratorW(hwnd, menu->accelerators, &msg);
            }
            if (!translated) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            detail::drain_posted_work();
        }

        return static_cast<int>(msg.wParam);
    }

} // namespace native
