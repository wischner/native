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

    void app_wnd::create() const {
        if (_created)
            return;

        validate_owner_created();
        windows::register_window_class();

        std::wstring title_w = windows::utf8_to_wide(_title);
        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
        const DWORD extended_style =
            get_modal() ? WS_EX_DLGMODALFRAME : 0;
        const DWORD style = WS_OVERLAPPEDWINDOW;
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
                                    const_cast<app_wnd *>(this));

        if (!hwnd)
            throw std::runtime_error(
                "Windows: Failed to create window.");

        windows::wnd_bindings.register_pair(
            hwnd, const_cast<app_wnd *>(this));
        _created = true;
        const_cast<app_wnd *>(this)->menu.attach(
            *const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_native_create();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "Windows: Cannot show window before it is created.");

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<app_wnd *>(this));
        if (!hwnd)
            throw std::runtime_error(
                "Windows: Missing HWND binding for app_wnd.");

        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
        if (get_modal() && owner_hwnd)
            EnableWindow(owner_hwnd, FALSE);

        ShowWindow(hwnd, SW_SHOW);
        if (get_modal()) {
            SetActiveWindow(hwnd);
            SetForegroundWindow(hwnd);
        }
        UpdateWindow(hwnd);
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        HWND owner_hwnd = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
        self->on_native_destroy();

        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_object(self);
        }

        if (get_modal() && owner && owner_hwnd) {
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
