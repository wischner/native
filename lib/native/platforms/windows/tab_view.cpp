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
}

namespace native
{
    void tab_view::apply_items() {
        HWND tabs = handle(this);
        if (!tabs)
            throw std::runtime_error("Windows: missing tab-view HWND.");
        TabCtrl_DeleteAllItems(tabs);
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            const std::wstring title = windows::utf8_to_wide(
                get_item(index).get_title());
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t *>(title.c_str());
            TabCtrl_InsertItem(tabs, static_cast<int>(index), &item);
        }
    }

    void tab_view::apply_selected_index() {
        HWND tabs = handle(this);
        if (!tabs)
            throw std::runtime_error("Windows: missing tab-view HWND.");
        TabCtrl_SetCurSel(tabs, get_selected_index());
    }

    void tab_view::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        HWND parent_window = parent
            ? windows::wnd_bindings.handle_from_object(parent)
            : nullptr;
        if (!parent || !parent->get_created() || !parent_window)
            throw std::runtime_error(
                "Windows: tab_view requires a created parent.");

        auto *self = const_cast<tab_view *>(this);
        HWND tabs = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN,
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
        _created = true;

        self->apply_items();
        RECT page{0, 0,
                  static_cast<LONG>(_bounds.d.w),
                  static_cast<LONG>(_bounds.d.h)};
        if (TabCtrl_AdjustRect(tabs, FALSE, &page))
            self->_tab_height = std::max(1L, page.top);
        self->refresh_contents();
        self->apply_selected_index();
        self->on_native_create();
    }

    void tab_view::show() const {
        HWND tabs = handle(const_cast<tab_view *>(this));
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

    void tab_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        HWND tabs = handle(self);
        self->on_native_destroy();
        if (tabs) {
            windows::wnd_bindings.unregister_by_handle(tabs);
            DestroyWindow(tabs);
        }
    }
} // namespace native
