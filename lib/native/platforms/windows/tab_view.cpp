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
#include <uxtheme.h>

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
        // The themed common control does not support all four placements.
        // Use its native classic renderer consistently, never owner drawing.
        SetWindowTheme(tabs, L"", L"");
        // The native minimum width leaves short vertical labels at one end
        // of a padded tab. Content-sized tabs retain symmetric native padding.
        const bool vertical = get_tab_placement() == tab_placement::left ||
                              get_tab_placement() == tab_placement::right;
        TabCtrl_SetMinTabWidth(tabs, vertical ? 0 : -1);
        TabCtrl_DeleteAllItems(tabs);
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            const std::wstring title = windows::utf8_to_wide(
                get_item(index).get_title());
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t *>(title.c_str());
            TabCtrl_InsertItem(tabs, static_cast<int>(index), &item);
        }
        const int inset = std::max(1, GetSystemMetrics(SM_CXEDGE) * 2);
        _page_inset = inset;
        _page_trailing = inset;
        _page_tab_gap = 0;
        RECT page{0, 0,
                  static_cast<LONG>(_bounds.d.w),
                  static_cast<LONG>(_bounds.d.h)};
        if (get_tab_placement() == tab_placement::top) {
            // TCM_ADJUSTRECT has no return value and supports only top tabs.
            TabCtrl_AdjustRect(tabs, FALSE, &page);
            _tab_height = std::max(1L, page.top);
            _page_inset = std::max(0L, page.left);
            _page_trailing = std::max(0L,
                static_cast<LONG>(_bounds.d.w) - page.right);
        } else {
            int extent = 0;
            for (int index = 0; index < TabCtrl_GetItemCount(tabs); ++index) {
                RECT item{};
                if (!TabCtrl_GetItemRect(tabs, index, &item))
                    continue;
                switch (get_tab_placement()) {
                case tab_placement::bottom:
                    extent = std::max(extent, static_cast<int>(
                        _bounds.d.h - item.top));
                    break;
                case tab_placement::left:
                    extent = std::max(extent, static_cast<int>(item.right));
                    break;
                case tab_placement::right:
                    extent = std::max(extent, static_cast<int>(
                        _bounds.d.w - item.left));
                    break;
                case tab_placement::top:
                    break;
                }
            }
            _tab_height = std::max(1, extent + inset);
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
