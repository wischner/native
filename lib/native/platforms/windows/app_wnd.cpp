//
// Implements the Windows application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>

#include <windows.h>

#include <native.h>
#include <native/app_wnd.h>

#include "globals.h"

namespace windows
{
    // Mirror the portable modal branch into native input enablement for
    // every top-level peer, including modeless siblings of the modal.
    static BOOL CALLBACK synchronize_modal_window(HWND hwnd, LPARAM) {
        auto *window = dynamic_cast<native::app_wnd *>(
            wnd_bindings.object_from_handle(hwnd));
        if (window)
            EnableWindow(hwnd, window->get_input_enabled());
        return TRUE;
    }

    const wchar_t class_name[] = L"native_window_class";

    void register_window_class() {
        static bool registered = false;
        if (registered)
            return;

        WNDCLASSW wc = {};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = routed_wnd_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClassW(&wc))
            throw std::runtime_error(
                "Windows: Failed to register window class.");

        registered = true;
    }
} // namespace windows

namespace native
{
    void app_wnd::apply_title() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: Missing HWND binding for app_wnd.");

        std::wstring wide = windows::utf8_to_wide(_title);
        SetWindowTextW(hwnd, wide.c_str());
    }

    void app_wnd::create_native() {
        validate_owner_created();
        windows::register_window_class();

        std::wstring title_w = windows::utf8_to_wide(_title);
        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
        // Keep the C++ lifetime/modal graph, but ordinary modeless windows
        // must stack independently. A Win32 owner would pin them above it.
        if (dynamic_cast<modeless_wnd *>(this))
            owner_hwnd = nullptr;
        const DWORD extended_style =
            get_modal() ? WS_EX_DLGMODALFRAME : 0;
        const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
        RECT framed_bounds = {
            0, 0, _bounds.d.w, _bounds.d.h};
        if (!AdjustWindowRectEx(
                &framed_bounds,
                style,
                !menu.tops().empty(),
                extended_style)) {
            throw std::runtime_error(
                "Windows: Failed to calculate the window frame.");
        }
        HWND hwnd = CreateWindowExW(extended_style,
                                    windows::class_name,
                                    title_w.c_str(),
                                    style,
                                    _bounds.p.x,
                                    _bounds.p.y,
                                    framed_bounds.right -
                                        framed_bounds.left,
                                    framed_bounds.bottom -
                                        framed_bounds.top,
                                    owner_hwnd,
                                    nullptr,
                                    GetModuleHandle(nullptr),
                                    this);

        if (!hwnd)
            throw std::runtime_error(
                "Windows: Failed to create window.");

        windows::wnd_bindings.register_pair(
            hwnd, this);
        this->menu.attach(
            *this);
    }

    void app_wnd::show_native() {
        if (!_created)
            throw std::runtime_error(
                "Windows: Cannot show window before it is created.");

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            this);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: Missing HWND binding for app_wnd.");

        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
        if (get_modal() && owner_hwnd)
            EnumThreadWindows(GetCurrentThreadId(),
                              windows::synchronize_modal_window, 0);

        ShowWindow(hwnd, SW_SHOW);
        if (get_modal()) {
            SetActiveWindow(hwnd);
            SetForegroundWindow(hwnd);
        }
        UpdateWindow(hwnd);
    }

    void app_wnd::destroy_native() {
        if (!_created)
            return;

        app_wnd *self = this;
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;

        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_object(self);
        }

        if (get_modal() && owner && owner_hwnd) {
            EnumThreadWindows(GetCurrentThreadId(),
                              windows::synchronize_modal_window, 0);
            if (owner->get_input_enabled()) {
                EnableWindow(owner_hwnd, TRUE);
                SetActiveWindow(owner_hwnd);
            } else if (modal_wnd *active =
                           owner->get_active_modal()) {
                HWND active_hwnd = windows::wnd_bindings
                                       .handle_from_object(active);
                if (active_hwnd)
                    SetActiveWindow(active_hwnd);
            }
        }
    }
} // namespace native
