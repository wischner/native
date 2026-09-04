//
// Implements tree_view with the Win32 common-controls Tree-View.
// Stable public IDs are mapped separately from pointer-sized native
// item data so the complete 64-bit identity contract is preserved.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <functional>
#include <stdexcept>

#include <windows.h>
#include <commctrl.h>

#include <native.h>

#include "../../control_render_access.h"
#include "globals.h"

namespace
{
    HBITMAP item_bitmap(const native::img *source,
                        native::size dimensions) {
        BITMAPINFO information{};
        information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        information.bmiHeader.biWidth = dimensions.w;
        information.bmiHeader.biHeight =
            -static_cast<LONG>(dimensions.h);
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
        if (!source)
            return bitmap;
        const double scale = std::min(
            {1.0,
             static_cast<double>(dimensions.w) / source->w(),
             static_cast<double>(dimensions.h) / source->h()});
        const int width = std::max(
            1, static_cast<int>(source->w() * scale));
        const int height = std::max(
            1, static_cast<int>(source->h() * scale));
        const int offset_x = (dimensions.w - width) / 2;
        const int offset_y = (dimensions.h - height) / 2;
        for (int y = 0; y < height; ++y) {
            const int source_y = std::min(
                source->h() - 1, y * source->h() / height);
            for (int x = 0; x < width; ++x) {
                const int source_x = std::min(
                    source->w() - 1, x * source->w() / width);
                const native::rgba color = source->pixels()[
                    source_y * source->w() + source_x];
                std::uint8_t *pixel = target +
                    (static_cast<std::size_t>(offset_y + y) *
                         dimensions.w +
                     offset_x + x) *
                        4;
                pixel[0] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.b) * color.a /
                    255);
                pixel[1] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.g) * color.a /
                    255);
                pixel[2] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(color.r) * color.a /
                    255);
                pixel[3] = color.a;
            }
        }
        return bitmap;
    }

    windows::win_tree_view &binding_for(native::tree_view &tree) {
        auto *binding =
            windows::tree_view_bindings.object_from_handle(&tree);
        if (!binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: missing tree_view binding.");
        return *binding;
    }

    LRESULT CALLBACK tree_view_proc(HWND window,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
        auto *tree = dynamic_cast<native::tree_view *>(
            windows::wnd_bindings.object_from_handle(window));
        auto *binding = tree
                            ? windows::tree_view_bindings
                                  .object_from_handle(tree)
                            : nullptr;
        if (tree && message == WM_KEYDOWN && wparam == VK_SPACE) {
            tree->on_native_navigation(
                native::tree_view_navigation::toggle);
            return 0;
        }
        return binding && binding->original_proc
                   ? CallWindowProcW(binding->original_proc,
                                     window,
                                     message,
                                     wparam,
                                     lparam)
                   : DefWindowProcW(window, message, wparam, lparam);
    }

    void rebuild(native::tree_view &tree) {
        windows::win_tree_view &binding = binding_for(tree);
        binding.suppress = true;
        TreeView_DeleteAllItems(binding.hwnd);
        binding.items.clear();
        binding.ids.clear();
        if (binding.images)
            ImageList_Destroy(binding.images);

        const native::size icon_size = tree.get_icon_size();
        binding.images = ImageList_Create(icon_size.w,
                                          icon_size.h,
                                          ILC_COLOR32,
                                          1,
                                          4);
        HBITMAP blank = item_bitmap(nullptr, icon_size);
        if (binding.images && blank) {
            ImageList_Add(binding.images, blank, nullptr);
            DeleteObject(blank);
        }
        TreeView_SetImageList(
            binding.hwnd, binding.images, TVSIL_NORMAL);

        LONG_PTR style = GetWindowLongPtrW(
            binding.hwnd, GWL_STYLE);
        if (tree.get_lines_visible())
            style |= TVS_HASLINES | TVS_LINESATROOT;
        else
            style &= ~(TVS_HASLINES | TVS_LINESATROOT);
        SetWindowLongPtrW(binding.hwnd, GWL_STYLE, style);

        std::function<void(const std::vector<native::tree_view_item> &,
                           HTREEITEM)> append;
        append = [&tree, &binding, &append](
                     const std::vector<native::tree_view_item> &items,
                     HTREEITEM parent) {
            for (const native::tree_view_item &value : items) {
                int image_index = 0;
                if (value.image && binding.images) {
                    HBITMAP bitmap = item_bitmap(
                        value.image.get(), tree.get_icon_size());
                    if (bitmap) {
                        image_index = ImageList_Add(
                            binding.images, bitmap, nullptr);
                        DeleteObject(bitmap);
                    }
                }
                std::wstring text =
                    windows::utf8_to_wide(value.text);
                TVINSERTSTRUCTW insert{};
                insert.hParent = parent;
                insert.hInsertAfter = TVI_LAST;
                insert.item.mask = TVIF_TEXT | TVIF_IMAGE |
                                   TVIF_SELECTEDIMAGE;
                insert.item.pszText = text.data();
                insert.item.iImage = image_index;
                insert.item.iSelectedImage = image_index;
                HTREEITEM native_item = reinterpret_cast<HTREEITEM>(
                    SendMessageW(binding.hwnd,
                                 TVM_INSERTITEMW,
                                 0,
                                 reinterpret_cast<LPARAM>(&insert)));
                if (!native_item)
                    continue;
                binding.items[value.id] = native_item;
                binding.ids[native_item] = value.id;
                append(value.children, native_item);
                if (value.expanded)
                    TreeView_Expand(
                        binding.hwnd, native_item, TVE_EXPAND);
            }
        };
        append(tree.get_items(), TVI_ROOT);
        binding.suppress = false;
    }

    native::tree_item_id id_for_item(
        const windows::win_tree_view &binding,
        HTREEITEM item) {
        const auto found = binding.ids.find(item);
        return found == binding.ids.end()
                   ? native::invalid_tree_item_id
                   : found->second;
    }
} // namespace

namespace windows
{
    LRESULT handle_tree_notify(native::tree_view *tree,
                               NMHDR *notification) {
        auto *binding = tree
                            ? tree_view_bindings.object_from_handle(tree)
                            : nullptr;
        if (!tree || !binding || binding->suppress || !notification)
            return 0;
        if (notification->code == NM_SETFOCUS) {
            tree->on_native_focus(true);
            return 0;
        }
        if (notification->code == NM_KILLFOCUS) {
            tree->on_native_focus(false);
            return 0;
        }
        if (notification->code == NM_CUSTOMDRAW) {
            auto *drawing = reinterpret_cast<NMTVCUSTOMDRAW *>(
                notification);
            windows::scoped_gpx_dc custom_draw_context(
                tree->get_gpx(), drawing->nmcd.hdc);
            if (drawing->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;
            if (drawing->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const auto item = reinterpret_cast<HTREEITEM>(
                    drawing->nmcd.dwItemSpec);
                const native::tree_item_id id =
                    id_for_item(*binding, item);
                if (id == native::invalid_tree_item_id)
                    return CDRF_DODEFAULT;
                std::size_t visible_index = 0;
                for (; visible_index < tree->get_visible_item_count();
                     ++visible_index) {
                    if (tree->get_visible_item(visible_index).id == id)
                        break;
                }
                if (visible_index >= tree->get_visible_item_count())
                    return CDRF_DODEFAULT;
                RECT native_bounds{};
                if (!TreeView_GetItemRect(binding->hwnd,
                                          item,
                                          &native_bounds,
                                          FALSE)) {
                    return CDRF_DODEFAULT;
                }
                RECT client{};
                GetClientRect(binding->hwnd, &client);
                native_bounds.left = client.left;
                native_bounds.right = client.right;
                native::rect bounds(
                    native_bounds.left,
                    native_bounds.top,
                    native_bounds.right - native_bounds.left,
                    native_bounds.bottom - native_bounds.top);
                native::gpx &graphics =
                    tree->get_gpx().set_clip(bounds);
                auto appearance = native::theme::create(graphics);
                native::theme::state state;
                state.selected =
                    (drawing->nmcd.uItemState & CDIS_SELECTED) != 0;
                state.focused =
                    (drawing->nmcd.uItemState & CDIS_FOCUS) != 0;
                state.hot =
                    (drawing->nmcd.uItemState & CDIS_HOT) != 0;
                state.disabled = !tree->get_item(id).enabled;
                const native::tree_view_visible_item visible =
                    tree->get_visible_item(visible_index);
                native::detail::control_render_access::draw_tree_row(
                    *tree,
                    graphics,
                    *appearance,
                    visible_index,
                    tree->get_item(id),
                    visible.depth,
                    bounds,
                    state);
                return CDRF_SKIPDEFAULT;
            }
            return CDRF_DODEFAULT;
        }
        if (notification->code == TVN_SELCHANGEDW ||
            notification->code == TVN_SELCHANGEDA) {
            HTREEITEM selected = nullptr;
            if (notification->code == TVN_SELCHANGEDW) {
                selected = reinterpret_cast<NMTREEVIEWW *>(
                               notification)
                               ->itemNew.hItem;
            } else {
                selected = reinterpret_cast<NMTREEVIEWA *>(
                               notification)
                               ->itemNew.hItem;
            }
            const native::tree_item_id id =
                id_for_item(*binding, selected);
            if (id != native::invalid_tree_item_id &&
                !tree->get_item(id).enabled) {
                binding->suppress = true;
                const auto previous = binding->items.find(
                    tree->get_selected_item());
                TreeView_SelectItem(
                    binding->hwnd,
                    previous == binding->items.end()
                        ? nullptr
                        : previous->second);
                binding->suppress = false;
            } else {
                tree->on_native_selection(id);
            }
            return 0;
        }
        if (notification->code == TVN_ITEMEXPANDEDW ||
            notification->code == TVN_ITEMEXPANDEDA) {
            HTREEITEM item = nullptr;
            UINT action = 0;
            if (notification->code == TVN_ITEMEXPANDEDW) {
                auto *expanded = reinterpret_cast<NMTREEVIEWW *>(
                    notification);
                item = expanded->itemNew.hItem;
                action = expanded->action;
            } else {
                auto *expanded = reinterpret_cast<NMTREEVIEWA *>(
                    notification);
                item = expanded->itemNew.hItem;
                action = expanded->action;
            }
            const native::tree_item_id id =
                id_for_item(*binding, item);
            if (id != native::invalid_tree_item_id) {
                if (!tree->get_item(id).enabled) {
                    binding->suppress = true;
                    TreeView_Expand(
                        binding->hwnd,
                        item,
                        tree->get_expanded(id)
                            ? TVE_EXPAND
                            : TVE_COLLAPSE);
                    binding->suppress = false;
                    return 0;
                }
                tree->on_native_expansion(
                    id, (action & TVE_EXPAND) != 0);
            }
            return 0;
        }
        if (notification->code == NM_DBLCLK ||
            notification->code == NM_RETURN) {
            const native::tree_item_id id =
                tree->get_selected_item();
            if (id != native::invalid_tree_item_id)
                tree->on_native_activate(id);
            return 0;
        }
        return 0;
    }
} // namespace windows

namespace native
{
    void tree_view::apply_items() {
        auto &binding = binding_for(*this);
        LONG_PTR extended = GetWindowLongPtrW(
            binding.hwnd, GWL_EXSTYLE);
        if (get_border_visible())
            extended |= WS_EX_CLIENTEDGE;
        else
            extended &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtrW(binding.hwnd, GWL_EXSTYLE, extended);
        SetWindowPos(binding.hwnd,
                     nullptr,
                     0,
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE | SWP_FRAMECHANGED);
        rebuild(*this);
        TreeView_SetIndent(binding.hwnd,
                           std::max<int>(
                               get_icon_size().w + 3, 16));
    }

    void tree_view::apply_selection() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        const auto found = binding.items.find(_selected_item);
        TreeView_SelectItem(
            binding.hwnd,
            found == binding.items.end() ? nullptr : found->second);
        if (found != binding.items.end())
            TreeView_EnsureVisible(binding.hwnd, found->second);
        binding.suppress = false;
    }

    void tree_view::apply_expansion(tree_item_id id, bool expanded) {
        auto &binding = binding_for(*this);
        const auto found = binding.items.find(id);
        if (found == binding.items.end())
            return;
        binding.suppress = true;
        TreeView_Expand(binding.hwnd,
                        found->second,
                        expanded ? TVE_EXPAND : TVE_COLLAPSE);
        binding.suppress = false;
    }

    void tree_view::apply_scroll_offset() {
        auto &binding = binding_for(*this);
        if (get_visible_item_count() == 0)
            return;
        const int row_height = std::max<int>(
            1, get_row_bounds(0).d.h);
        const std::size_t row = std::min(
            get_visible_item_count() - 1,
            static_cast<std::size_t>(
                std::max(0, _scroll_offset) / row_height));
        const tree_item_id id = get_visible_item(row).id;
        const auto found = binding.items.find(id);
        if (found != binding.items.end())
            TreeView_Select(
                binding.hwnd, found->second, TVGN_FIRSTVISIBLE);
    }

    void tree_view::create_native() {
        wnd *parent = get_parent();
        HWND parent_hwnd = parent
                               ? windows::wnd_bindings.handle_from_object(
                                     parent)
                               : nullptr;
        if (!parent || !parent->get_created() || !parent_hwnd)
            throw std::runtime_error(
                "Windows: tree_view requires a created parent.");
        INITCOMMONCONTROLSEX controls{
            sizeof(controls), ICC_TREEVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        auto *self = this;
        DWORD style = WS_CHILD | WS_TABSTOP | WS_VSCROLL |
                      TVS_SHOWSELALWAYS | TVS_HASBUTTONS;
        if (_lines_visible)
            style |= TVS_HASLINES | TVS_LINESATROOT;
        HWND hwnd = CreateWindowExW(get_border_visible()
                                        ? WS_EX_CLIENTEDGE
                                        : 0,
                                    WC_TREEVIEWW,
                                    L"",
                                    style,
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
                "Windows: failed to create Tree-View tree_view.");
        windows::wnd_bindings.register_pair(hwnd, self);
        auto *binding = new windows::win_tree_view();
        binding->hwnd = hwnd;
        windows::tree_view_bindings.register_pair(self, binding);
        binding->original_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd,
                              GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(
                                  tree_view_proc)));
        SendMessageW(hwnd,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        self->synchronize_theme_metrics();
        self->apply_items();
        self->apply_selection();
    }

    void tree_view::show_native() {
        auto *binding = windows::tree_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: tree_view is not created.");
        ShowWindow(binding->hwnd, SW_SHOW);
        UpdateWindow(binding->hwnd);
    }

    void tree_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            windows::tree_view_bindings.object_from_handle(self);
        if (binding) {
            if (binding->images)
                ImageList_Destroy(binding->images);
            if (binding->hwnd) {
                DestroyWindow(binding->hwnd);
                windows::wnd_bindings.unregister_by_handle(
                    binding->hwnd);
            }
            windows::tree_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
