//
// Implements tab_view with the Win32 tab common control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <windows.h>
#include <commctrl.h>

#include <native.h>

#include "globals.h"

namespace
{
    HWND handle(native::tab_view *owner) {
        return windows::wnd_bindings.handle_from_object(owner);
    }

    LONG_PTR placement_style(native::tab_placement placement) {
        switch (placement) {
        case native::tab_placement::top:
            return 0;
        case native::tab_placement::bottom:
            return TCS_BOTTOM;
        case native::tab_placement::left:
            return TCS_VERTICAL | TCS_MULTILINE;
        case native::tab_placement::right:
            return TCS_VERTICAL | TCS_MULTILINE | TCS_RIGHT;
        }
        return 0;
    }
}

namespace native
{
    void tab_view::apply_items() {
        HWND tabs = handle(this);
        if (!tabs)
            throw std::runtime_error("Windows: missing tab-view HWND.");
        LONG_PTR style = GetWindowLongPtrW(tabs, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(
            TCS_BOTTOM | TCS_VERTICAL | TCS_MULTILINE);
        style |= placement_style(get_tab_placement());
        SetWindowLongPtrW(tabs, GWL_STYLE, style);
        SetWindowPos(tabs, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE | SWP_FRAMECHANGED);
        TabCtrl_DeleteAllItems(tabs);
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            const std::wstring title = windows::utf8_to_wide(
                get_item(index).get_title());
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t *>(title.c_str());
            TabCtrl_InsertItem(tabs, static_cast<int>(index), &item);
        }
        RECT page{0, 0,
                  static_cast<LONG>(_bounds.d.w),
                  static_cast<LONG>(_bounds.d.h)};
        if (TabCtrl_AdjustRect(tabs, FALSE, &page)) {
            switch (get_tab_placement()) {
            case tab_placement::top:
                _tab_height = std::max(1L, page.top);
                break;
            case tab_placement::bottom:
                _tab_height = std::max(
                    1L,
                    static_cast<LONG>(_bounds.d.h) - page.bottom);
                break;
            case tab_placement::left:
                _tab_height = std::max(1L, page.left);
                break;
            case tab_placement::right:
                _tab_height = std::max(
                    1L,
                    static_cast<LONG>(_bounds.d.w) - page.right);
                break;
            }
        }
        InvalidateRect(tabs, nullptr, TRUE);
    }

    void tab_view::apply_selected_index() {
        HWND tabs = handle(this);
        if (!tabs)
            throw std::runtime_error("Windows: missing tab-view HWND.");
        TabCtrl_SetCurSel(tabs, get_selected_index());
    }

    void tab_view::create_native() {
        wnd *parent = get_parent();
        HWND parent_window = parent
            ? windows::wnd_bindings.handle_from_object(parent)
            : nullptr;
        if (!parent || !parent->get_created() || !parent_window)
            throw std::runtime_error(
                "Windows: tab_view requires a created parent.");

        auto *self = this;
        HWND tabs = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN |
                placement_style(get_tab_placement()),
            _bounds.p.x,
            _bounds.p.y,
            _bounds.d.w,
            _bounds.d.h,
            parent_window,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr);
        if (!tabs)
            throw std::runtime_error(
                "Windows: failed to create tab view.");
        SendMessageW(tabs, WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        windows::wnd_bindings.register_pair(tabs, self);

        self->apply_items();
        self->refresh_contents();
        self->apply_selected_index();
    }

    void tab_view::show_native() {
        HWND tabs = handle(this);
        if (!_created || !tabs)
            throw std::runtime_error("Windows: tab_view is not created.");
        ShowWindow(tabs, SW_SHOW);
        UpdateWindow(tabs);
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created())
                content.show();
        }
    }

    void tab_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        HWND tabs = handle(self);
        if (tabs) {
            windows::wnd_bindings.unregister_by_handle(tabs);
            DestroyWindow(tabs);
        }
    }
} // namespace native
