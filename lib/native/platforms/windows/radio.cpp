//
// Implements the native Win32 radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <windows.h>
#include <native.h>
#include <native/radio.h>
#include "globals.h"
namespace native
{
    void radio::apply_text() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing radio HWND.");
        auto s = windows::utf8_to_wide(_text);
        SetWindowTextW(h, s.c_str());
    }
    void radio::apply_selected() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing radio HWND.");
        SendMessageW(
            h, BM_SETCHECK, _selected ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    void radio::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        HWND parent =
            p ? windows::wnd_bindings.handle_from_object(p) : nullptr;
        if (!p || !p->get_created() || !parent)
            throw std::runtime_error(
                "Windows: radio requires a created parent.");
        auto *self = const_cast<radio *>(this);
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
                "Windows: Failed to create radio.");
        windows::wnd_bindings.register_pair(h, self);
        SendMessageW(h,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        SendMessageW(
            h, BM_SETCHECK, _selected ? BST_CHECKED : BST_UNCHECKED, 0);
        _created = true;
        self->on_native_create();
    }
    void radio::show() const {
        HWND h = windows::wnd_bindings.handle_from_object(
            const_cast<radio *>(this));
        if (!_created || !h)
            throw std::runtime_error("Windows: radio is not created.");
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        HWND h = windows::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (h) {
            DestroyWindow(h);
            windows::wnd_bindings.unregister_by_handle(h);
        }
    }
} // namespace native
