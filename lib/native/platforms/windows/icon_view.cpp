//
// Implements icon_view with the Win32 common-controls List-View.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

#include <native.h>

#include "globals.h"

namespace
{
    HBITMAP item_bitmap(const native::img &source,
                        native::size dimensions) {
        BITMAPINFO information{};
        information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        information.bmiHeader.biWidth = dimensions.w;
        information.bmiHeader.biHeight = -static_cast<LONG>(dimensions.h);
        information.bmiHeader.biPlanes = 1;
        information.bmiHeader.biBitCount = 32;
        information.bmiHeader.biCompression = BI_RGB;
        void *bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(nullptr,
                                          &information,
                                          DIB_RGB_COLORS,
                                          &bits,
                                          nullptr,
                                          0);
        if (!bitmap || !bits)
            return bitmap;
        auto *target = static_cast<std::uint8_t *>(bits);
        std::fill(target,
                  target + static_cast<std::size_t>(dimensions.w) *
                               dimensions.h * 4,
                  0);
        const double scale = std::min(
            {1.0,
             static_cast<double>(dimensions.w) / source.w(),
             static_cast<double>(dimensions.h) / source.h()});
        const int width = std::max(1, static_cast<int>(source.w() * scale));
        const int height =
            std::max(1, static_cast<int>(source.h() * scale));
        const int offset_x = (dimensions.w - width) / 2;
        const int offset_y = (dimensions.h - height) / 2;
        for (int y = 0; y < height; ++y) {
            const int source_y = std::min(
                source.h() - 1, y * source.h() / height);
            for (int x = 0; x < width; ++x) {
                const int source_x = std::min(
                    source.w() - 1, x * source.w() / width);
                const native::rgba color =
                    source.pixels()[source_y * source.w() + source_x];
                std::uint8_t *pixel = target +
                    (static_cast<std::size_t>(offset_y + y) *
                         dimensions.w +
                     offset_x + x) *
                        4;
                pixel[0] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.b) * color.a / 255);
                pixel[1] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.g) * color.a / 255);
                pixel[2] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.r) * color.a / 255);
                pixel[3] = color.a;
            }
        }
        return bitmap;
    }

    windows::win_icon_view &binding_for(native::icon_view &control) {
        auto *binding =
            windows::icon_view_bindings.object_from_handle(&control);
        if (!binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: missing icon_view binding.");
        return *binding;
    }

    void rebuild(native::icon_view &control) {
        windows::win_icon_view &binding = binding_for(control);
        binding.suppress = true;
        ListView_DeleteAllItems(binding.hwnd);
        if (binding.images)
            ImageList_Destroy(binding.images);
        const native::size icon_size = control.get_icon_size();
        binding.images = ImageList_Create(icon_size.w,
                                          icon_size.h,
                                          ILC_COLOR32,
                                          static_cast<int>(
                                              control.get_items().size()),
                                          4);
        ListView_SetImageList(binding.hwnd,
                              binding.images,
                              LVSIL_NORMAL);
        const auto &items = control.get_items();
        for (std::size_t index = 0; index < items.size(); ++index) {
            int image_index = -1;
            if (items[index].image) {
                HBITMAP bitmap = item_bitmap(
                    *items[index].image, icon_size);
                if (bitmap) {
                    image_index = ImageList_Add(
                        binding.images, bitmap, nullptr);
                    DeleteObject(bitmap);
                }
            }
            std::wstring label = control.get_label_mode() ==
                                         native::icon_view_label_mode::hidden
                                     ? std::wstring()
                                     : windows::utf8_to_wide(
                                           items[index].text);
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            item.iItem = static_cast<int>(index);
            item.iImage = image_index;
            item.lParam = static_cast<LPARAM>(items[index].id);
            item.pszText = label.data();
            ListView_InsertItem(binding.hwnd, &item);
        }
        binding.suppress = false;
    }
} // namespace

namespace native
{
    void icon_view::apply_items() { rebuild(*this); }

    void icon_view::apply_icon_size() {
        rebuild(*this);
        ListView_SetIconSpacing(binding_for(*this).hwnd,
                                std::max<int>(get_icon_size().w + 32, 80),
                                get_icon_size().h + 44);
    }

    void icon_view::apply_label_mode() { rebuild(*this); }

    void icon_view::apply_selected_index() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        ListView_SetItemState(binding.hwnd,
                              -1,
                              0,
                              LVIS_SELECTED | LVIS_FOCUSED);
        if (_selected_index >= 0) {
            ListView_SetItemState(binding.hwnd,
                                  _selected_index,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(
                binding.hwnd, _selected_index, FALSE);
        }
        binding.suppress = false;
    }

    void icon_view::apply_scroll_offset() {
        auto &binding = binding_for(*this);
        const int delta = _scroll_offset - binding.applied_scroll;
        if (delta != 0) {
            ListView_Scroll(binding.hwnd, 0, delta);
            binding.applied_scroll = _scroll_offset;
        }
    }

    void icon_view::create_native() {
        wnd *parent = get_parent();
        HWND parent_hwnd = parent
                               ? windows::wnd_bindings.handle_from_object(
                                     parent)
                               : nullptr;
        if (!parent || !parent->get_created() || !parent_hwnd)
            throw std::runtime_error(
                "Windows: icon_view requires a created parent.");
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        auto *self = this;
        HWND hwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            WS_CHILD | WS_TABSTOP | WS_VSCROLL | LVS_ICON |
                LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
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
                "Windows: failed to create List-View icon_view.");
        windows::wnd_bindings.register_pair(hwnd, self);
        SetWindowTheme(hwnd, L"Explorer", nullptr);
        ListView_SetExtendedListViewStyle(hwnd, LVS_EX_DOUBLEBUFFER);
        auto *binding = new windows::win_icon_view();
        binding->hwnd = hwnd;
        windows::icon_view_bindings.register_pair(self, binding);
        SendMessageW(hwnd,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_icon_size();
        self->apply_selected_index();
    }

    void icon_view::show_native() {
        auto *binding = windows::icon_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: icon_view is not created.");
        ShowWindow(binding->hwnd, SW_SHOW);
        UpdateWindow(binding->hwnd);
    }

    void icon_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            windows::icon_view_bindings.object_from_handle(self);
        if (binding) {
            if (binding->images)
                ImageList_Destroy(binding->images);
            if (binding->hwnd) {
                DestroyWindow(binding->hwnd);
                windows::wnd_bindings.unregister_by_handle(
                    binding->hwnd);
            }
            windows::icon_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
