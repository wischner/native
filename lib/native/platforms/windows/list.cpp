//
// Implements the native Win32 list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <windows.h>
#include <native.h>
#include <native/list.h>
#include "globals.h"
namespace
{
    void add_items(HWND h, const std::vector<std::string> &items) {
        for (const auto &i : items) {
            auto s = windows::utf8_to_wide(i);
            SendMessageW(h,
                         LB_ADDSTRING,
                         0,
                         reinterpret_cast<LPARAM>(s.c_str()));
        }
    }
} // namespace
namespace native
{
    void list::apply_items() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing list HWND.");
        SendMessageW(h, LB_RESETCONTENT, 0, 0);
        add_items(h, _items);
    }
    void list::apply_selected_index() {
        HWND h = windows::wnd_bindings.handle_from_object(this);
        if (!h)
            throw std::runtime_error("Windows: Missing list HWND.");
        SendMessageW(
            h, LB_SETCURSEL, static_cast<WPARAM>(_selected_index), 0);
    }
    void list::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        HWND parent =
            p ? windows::wnd_bindings.handle_from_object(p) : nullptr;
        if (!p || !p->get_created() || !parent)
            throw std::runtime_error(
                "Windows: list requires a created parent.");
        auto *self = const_cast<list *>(this);
        HWND h = CreateWindowExW(0,
                                 L"LISTBOX",
                                 L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                                     WS_BORDER | WS_VSCROLL,
                                 _bounds.p.x,
                                 _bounds.p.y,
                                 _bounds.d.w,
                                 _bounds.d.h,
                                 parent,
                                 nullptr,
                                 GetModuleHandle(nullptr),
                                 nullptr);
        if (!h)
            throw std::runtime_error("Windows: Failed to create list.");
        windows::wnd_bindings.register_pair(h, self);
        SendMessageW(h,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        add_items(h, _items);
        SendMessageW(
            h, LB_SETCURSEL, static_cast<WPARAM>(_selected_index), 0);
        _created = true;
        self->on_wnd_create.emit();
    }
    void list::show() const {
        HWND h = windows::wnd_bindings.handle_from_object(
            const_cast<list *>(this));
        if (!_created || !h)
            throw std::runtime_error("Windows: list is not created.");
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
    }
    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        HWND h = windows::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (h) {
            DestroyWindow(h);
            windows::wnd_bindings.unregister_by_handle(h);
        }
    }
} // namespace native
