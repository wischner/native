//
// Implements the Win32 EDIT backend with live portable validation and
// shared clipboard commands for single-line and multiline editors.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <windows.h>

#include "globals.h"

namespace
{
    // Convert portable newlines to the Win32 EDIT convention.
    std::wstring native_text(const native::text_edit &editor,
                             const std::string &text) {
        std::string converted;
        converted.reserve(text.size());
        for (char character : text) {
            if (character == '\n' &&
                editor.get_mode() ==
                    native::text_edit_mode::multi_line) {
                converted += "\r\n";
            } else {
                converted.push_back(character);
            }
        }
        return windows::utf8_to_wide(converted);
    }

    // Convert Win32 EDIT newlines to portable line feeds.
    std::string portable_text(const std::wstring &text) {
        std::string converted = windows::wide_to_utf8(text);
        converted.erase(
            std::remove(converted.begin(), converted.end(), '\r'),
            converted.end());
        return converted;
    }

    // Read a complete Win32 control value.
    std::wstring window_text(HWND window) {
        const int length = GetWindowTextLengthW(window);
        std::wstring result(static_cast<std::size_t>(length + 1),
                            L'\0');
        if (length != 0) {
            GetWindowTextW(window, result.data(), length + 1);
        }
        result.resize(static_cast<std::size_t>(length));
        return result;
    }

    // Route standard editing shortcuts through portable commands.
    LRESULT CALLBACK text_edit_proc(HWND window,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
        auto *editor = dynamic_cast<native::text_edit *>(
            windows::wnd_bindings.object_from_handle(window));
        auto *binding = editor
                            ? windows::text_edit_bindings
                                  .object_from_handle(editor)
                            : nullptr;
        if (editor && message == WM_KEYDOWN &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            switch (wparam) {
            case 'A':
                editor->select_all();
                return 0;
            case 'C':
                editor->copy();
                return 0;
            case 'X':
                editor->cut();
                return 0;
            case 'V':
                editor->paste();
                return 0;
            }
        }
        return binding && binding->original_proc
                   ? CallWindowProcW(binding->original_proc,
                                     window,
                                     message,
                                     wparam,
                                     lparam)
                   : DefWindowProcW(window, message, wparam, lparam);
    }
} // namespace

namespace windows
{
    void handle_text_edit_change(native::text_edit *editor) {
        auto *binding = editor
                            ? text_edit_bindings.object_from_handle(
                                  editor)
                            : nullptr;
        if (!binding || binding->suppress)
            return;

        const std::string candidate =
            portable_text(window_text(binding->hwnd));
        if (editor->on_native_text(candidate))
            return;

        binding->suppress = true;
        const std::wstring restored =
            native_text(*editor, editor->get_text());
        SetWindowTextW(binding->hwnd, restored.c_str());
        binding->suppress = false;
    }
} // namespace windows

namespace native
{
    void text_edit::apply_text() {
        auto *binding =
            windows::text_edit_bindings.object_from_handle(this);
        if (!binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: Missing text-edit binding.");
        binding->suppress = true;
        const std::wstring value = native_text(*this, _text);
        SetWindowTextW(binding->hwnd, value.c_str());
        binding->suppress = false;
    }

    void text_edit::apply_read_only() {
        HWND window = windows::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "Windows: Missing text-edit window.");
        SendMessageW(window, EM_SETREADONLY, _read_only, 0);
    }

    void text_edit::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        HWND parent_window = parent
                                 ? windows::wnd_bindings
                                       .handle_from_object(parent)
                                 : nullptr;
        if (!parent || !parent->get_created() || !parent_window)
            throw std::runtime_error(
                "Windows: text_edit requires a created parent.");

        const DWORD mode_style =
            _mode == text_edit_mode::multi_line
                ? ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                      WS_VSCROLL
                : ES_AUTOHSCROLL;
        const std::wstring value = native_text(*this, _text);
        HWND window = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            value.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | mode_style,
            _bounds.p.x,
            _bounds.p.y,
            _bounds.d.w,
            _bounds.d.h,
            parent_window,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (!window)
            throw std::runtime_error(
                "Windows: Failed to create text_edit.");

        auto *self = const_cast<text_edit *>(this);
        auto *binding = new windows::win_text_edit;
        binding->hwnd = window;
        windows::wnd_bindings.register_pair(window, self);
        windows::text_edit_bindings.register_pair(self, binding);
        binding->original_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(window,
                              GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(
                                  text_edit_proc)));
        SendMessageW(window,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(
                         windows::control_font()),
                     TRUE);
        SendMessageW(window, EM_SETREADONLY, _read_only, 0);
        _created = true;
        self->on_wnd_create.emit();
    }

    void text_edit::show() const {
        HWND window = windows::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (!_created || !window)
            throw std::runtime_error(
                "Windows: text_edit is not created.");
        ShowWindow(window, SW_SHOW);
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        HWND window = windows::wnd_bindings.handle_from_object(self);
        auto *binding =
            windows::text_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (window) {
            windows::wnd_bindings.unregister_by_handle(window);
            DestroyWindow(window);
        }
        windows::text_edit_bindings.unregister_by_handle(self);
        delete binding;
    }

    std::string text_edit::selected_text() const {
        HWND window = windows::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (!window)
            return {};
        DWORD begin = 0;
        DWORD end = 0;
        SendMessageW(window,
                     EM_GETSEL,
                     reinterpret_cast<WPARAM>(&begin),
                     reinterpret_cast<LPARAM>(&end));
        const std::wstring value = window_text(window);
        if (begin >= end || end > value.size())
            return {};
        return portable_text(value.substr(begin, end - begin));
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        HWND window = windows::wnd_bindings.handle_from_object(this);
        if (!window || _read_only)
            return false;
        DWORD begin = 0;
        DWORD end = 0;
        SendMessageW(window,
                     EM_GETSEL,
                     reinterpret_cast<WPARAM>(&begin),
                     reinterpret_cast<LPARAM>(&end));
        std::wstring value = window_text(window);
        const std::wstring replacement = native_text(*this, text);
        value.replace(begin, end - begin, replacement);
        if (!validate(portable_text(value)))
            return false;
        SendMessageW(window,
                     EM_REPLACESEL,
                     TRUE,
                     reinterpret_cast<LPARAM>(replacement.c_str()));
        return true;
    }

    void text_edit::select_all_native() const {
        HWND window = windows::wnd_bindings.handle_from_object(
            const_cast<text_edit *>(this));
        if (window)
            SendMessageW(window, EM_SETSEL, 0, -1);
    }
} // namespace native
