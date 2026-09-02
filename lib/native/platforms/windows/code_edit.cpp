//
// Implements a native Win32 child host for the shared code editor.
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
    void code_edit::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        HWND parent_hwnd = parent
                               ? windows::wnd_bindings.handle_from_object(
                                     parent)
                               : nullptr;
        if (!parent || !parent->get_created() || !parent_hwnd)
            throw std::runtime_error(
                "Windows: code_edit requires a created parent.");
        windows::register_window_class();
        auto *self = const_cast<code_edit *>(this);
        HWND hwnd = CreateWindowExW(
            0,
            windows::class_name,
            L"",
            WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN,
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
                "Windows: failed to create code_edit host.");
        _created = true;
        self->invalidate();
        self->on_native_create();
    }

    void code_edit::show() const {
        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<code_edit *>(this));
        if (!_created || !hwnd)
            throw std::runtime_error(
                "Windows: code_edit is not created.");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        windows::code_edit_high_surrogates.erase(self);
        self->on_native_destroy();
        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_handle(hwnd);
        }
    }
} // namespace native
