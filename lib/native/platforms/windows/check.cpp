//
// Implements the native Win32 check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <windows.h>
#include <native.h>
#include <native/check.h>
#include "globals.h"
namespace native
{
    void check::apply_text() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing check HWND.");
        auto s = windows::utf8_to_wide(_text);
        SetWindowTextW(h, s.c_str());
    }
    void check::apply_checked() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing check HWND.");
        SendMessageW(
            h, BM_SETCHECK, _checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    void check::create_native() {
        auto *p = get_parent();
        HWND parent =
            p ? windows::wnd_bindings.handle_from_object(p) : nullptr;
        if (!p || !p->get_created() || !parent)
            throw std::runtime_error(
                "Windows: check requires a created parent.");
        auto *self = this;
        auto s = windows::utf8_to_wide(_text);
        HWND h = CreateWindowExW(0,
                                 L"BUTTON",
                                 s.c_str(),
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     BS_OWNERDRAW,
                                 _bounds.p.x,
                                 _bounds.p.y,
                                 _bounds.d.w,
                                 _bounds.d.h,
                                 parent,
                                 nullptr,
                                 GetModuleHandle(nullptr),
                                 nullptr);
        if (!h)
            throw std::runtime_error(
                "Windows: Failed to create check.");
        windows::wnd_bindings.register_pair(h, self);
        SendMessageW(h,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        SendMessageW(
            h, BM_SETCHECK, _checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    void check::show_native() {
        HWND h = windows::wnd_bindings.handle_from_object(
            this);
        if (!_created || !h)
            throw std::runtime_error("Windows: check is not created.");
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
    }
    void check::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        HWND h = windows::wnd_bindings.handle_from_object(self);
        if (h) {
            DestroyWindow(h);
            windows::wnd_bindings.unregister_by_handle(h);
        }
    }
} // namespace native
