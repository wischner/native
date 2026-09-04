//
// Implements the native Win32 combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <windows.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    HWND handle(native::combo_box *owner) {
        return windows::wnd_bindings.handle_from_object(owner);
    }

    void add_items(HWND window,
                   const std::vector<std::string> &items) {
        SendMessageW(window, CB_RESETCONTENT, 0, 0);
        for (const auto &item : items) {
            const std::wstring text = windows::utf8_to_wide(item);
            SendMessageW(window, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    HWND create_window(native::combo_box *owner) {
        native::wnd *parent = owner->get_parent();
        HWND parent_window = parent
            ? windows::wnd_bindings.handle_from_object(parent)
            : nullptr;
        if (!parent || !parent->get_created() || !parent_window)
            throw std::runtime_error(
                "Windows: combo box requires a created parent.");
        const DWORD style = WS_CHILD | WS_TABSTOP | WS_VSCROLL |
            (owner->get_style() == native::combo_box_style::editable
                ? CBS_DROPDOWN : CBS_DROPDOWNLIST);
        const native::rect bounds = owner->get_bounds();
        HWND window = CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            style,
            bounds.p.x,
            bounds.p.y,
            bounds.d.w,
            std::max<int>(bounds.d.h, 200),
            parent_window,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr);
        if (!window)
            throw std::runtime_error(
                "Windows: Failed to create combo box.");
        SendMessageW(window, WM_SETFONT,
            reinterpret_cast<WPARAM>(windows::control_font()), TRUE);
        SendMessageW(window, CB_SETMINVISIBLE, 8, 0);
        add_items(window, owner->get_items());
        SendMessageW(window, CB_SETCURSEL,
                     static_cast<WPARAM>(owner->get_selected_index()),
                     0);
        if (owner->get_style() ==
            native::combo_box_style::editable) {
            const std::wstring text =
                windows::utf8_to_wide(owner->get_text());
            SetWindowTextW(window, text.c_str());
        }
        return window;
    }
}

namespace native
{
    void combo_box::apply_items() {
        HWND window = handle(this);
        if (!window)
            throw std::runtime_error("Windows: Missing combo box HWND.");
        add_items(window, get_items());
    }

    void combo_box::apply_selected_index() {
        HWND window = handle(this);
        if (!window)
            throw std::runtime_error("Windows: Missing combo box HWND.");
        SendMessageW(window, CB_SETCURSEL,
                     static_cast<WPARAM>(get_selected_index()), 0);
    }

    void combo_box::apply_text() {
        HWND window = handle(this);
        if (!window)
            throw std::runtime_error("Windows: Missing combo box HWND.");
        const std::wstring text = windows::utf8_to_wide(get_text());
        SetWindowTextW(window, text.c_str());
    }

    void combo_box::apply_style() {
        HWND current = handle(this);
        if (!current)
            throw std::runtime_error("Windows: Missing combo box HWND.");
        const bool visible = IsWindowVisible(current) != FALSE;
        HWND replacement = create_window(this);
        try {
            windows::wnd_bindings.register_pair(replacement, this);
        } catch (...) {
            DestroyWindow(replacement);
            throw;
        }
        DestroyWindow(current);
        if (visible) {
            ShowWindow(replacement, SW_SHOW);
            UpdateWindow(replacement);
        }
    }

    void combo_box::create_native() {
        auto *self = this;
        HWND window = create_window(self);
        windows::wnd_bindings.register_pair(window, self);
    }

    void combo_box::show_native() {
        HWND window = handle(this);
        if (!_created || !window)
            throw std::runtime_error("Windows: combo box is not created.");
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        HWND window = handle(self);
        if (window) {
            windows::wnd_bindings.unregister_by_handle(window);
            DestroyWindow(window);
        }
    }
} // namespace native
