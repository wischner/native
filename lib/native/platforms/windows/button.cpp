//
// Implements the Windows button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>
#include <utility>
#include <typeinfo>

#include <windows.h>

#include <native.h>
#include <native/button.h>

#include "globals.h"

namespace native
{
    void button::apply_text() {
        auto *binding =
            windows::button_bindings.object_from_handle(this);
        if (!binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: Missing HWND binding for button.");

        std::wstring wide = windows::utf8_to_wide(_text);
        SetWindowTextW(binding->hwnd, wide.c_str());
    }

    void button::create_native() {
        wnd *p = get_parent();
        if (!p)
            throw std::runtime_error(
                "Windows: button requires a parent window.");
        if (!p->get_created())
            throw std::runtime_error(
                "Windows: button parent is not created.");

        HWND parent_hwnd = windows::wnd_bindings.handle_from_object(p);
        if (!parent_hwnd)
            throw std::runtime_error(
                "Windows: button parent is not created.");

        std::wstring text_w = windows::utf8_to_wide(_text);
        HWND hwnd =
            CreateWindowExW(0,
                            L"BUTTON",
                            text_w.c_str(),
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                (typeid(*this) == typeid(button)
                                     ? BS_PUSHBUTTON : BS_OWNERDRAW),
                            _bounds.p.x,
                            _bounds.p.y,
                            _bounds.d.w,
                            _bounds.d.h,
                            parent_hwnd,
                            nullptr,
                            GetModuleHandle(nullptr),
                            nullptr);

        if (!hwnd)
            throw std::runtime_error(
                "Windows: Failed to create button.");

        auto *self = this;
        windows::wnd_bindings.register_pair(hwnd, self);

        auto *h = new windows::win_button();
        h->hwnd = hwnd;
        h->owner = self;
        windows::button_bindings.register_pair(self, h);
        SendMessageW(hwnd,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);

    }

    void button::show_native() {
        if (!_created)
            throw std::runtime_error(
                "Windows: Cannot show button before it is created.");

        auto *h = windows::button_bindings.object_from_handle(
            this);
        if (!h || !h->hwnd)
            throw std::runtime_error(
                "Windows: Missing HWND binding for button.");

        ShowWindow(h->hwnd, SW_SHOW);
        UpdateWindow(h->hwnd);
    }

    void button::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *h = windows::button_bindings.object_from_handle(self);

        if (h) {
            if (h->hwnd) {
                DestroyWindow(h->hwnd);
                windows::wnd_bindings.unregister_by_handle(h->hwnd);
            }
            windows::button_bindings.unregister_by_handle(self);
            delete h;
        }
    }
} // namespace native
