//
// Implements a Win32-native-look accordion in a routed child host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <windows.h>

#include <native.h>

#include "globals.h"

namespace native
{
    void accordion::apply_items() { invalidate(); }

    void accordion::create_native() {
        wnd *parent = get_parent();
        HWND parent_hwnd = parent
                               ? windows::wnd_bindings.handle_from_object(
                                     parent)
                               : nullptr;
        if (!parent || !parent->get_created() || !parent_hwnd)
            throw std::runtime_error(
                "Windows: accordion requires a created parent.");
        windows::register_window_class();
        auto *self = this;
        HWND hwnd = CreateWindowExW(0,
                                    windows::class_name,
                                    L"",
                                    WS_CHILD | WS_TABSTOP |
                                        WS_CLIPCHILDREN,
                                    _bounds.p.x,
                                    _bounds.p.y,
                                    _bounds.d.w,
                                    _bounds.d.h,
                                    parent_hwnd,
                                    nullptr,
                                    GetModuleHandle(nullptr),
                                    self);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: failed to create accordion host.");
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void accordion::show_native() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(
            this);
        if (!_created || !hwnd)
            throw std::runtime_error(
                "Windows: accordion is not created.");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    void accordion::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_handle(hwnd);
        }
    }
} // namespace native
