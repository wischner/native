//
// Implements table_view with the Win32 report ListView. Virtual mode
// uses LVS_OWNERDATA; explicit materialized mode uses native groups.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>
#include <commctrl.h>

#include <native.h>

#include "../../control_render_access.h"
#include "../../table_visible_rows.h"
#include "globals.h"

namespace
{
    windows::win_table_view &binding_for(native::table_view &table) {
        auto *binding =
            windows::table_view_bindings.object_from_handle(&table);
        if (!binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: missing table_view binding.");
        return *binding;
    }

    HBITMAP table_bitmap(const native::img &source,
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
        const int width = std::max(
            1, static_cast<int>(source.w() * scale));
        const int height = std::max(
            1, static_cast<int>(source.h() * scale));
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
                         dimensions.w + offset_x + x) * 4;
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

    native::size icon_dimensions(native::table_view &table) {
        return table.get_icon_size().value_or(native::size(16, 16));
    }

    int image_index(native::table_view &table,
                    const native::img *image) {
        if (!image)
            return -1;
        auto &binding = binding_for(table);
        const auto cached = binding.image_indexes.find(image);
        if (cached != binding.image_indexes.end())
            return cached->second;
        HBITMAP bitmap = table_bitmap(*image, icon_dimensions(table));
        if (!bitmap)
            return -1;
        const int index = ImageList_Add(binding.images, bitmap, nullptr);
        DeleteObject(bitmap);
        if (index >= 0)
            binding.image_indexes[image] = index;
        return index;
    }

    std::optional<native::table_group> group_by_id(
        native::table_model &model,
        native::table_group_id id) {
        for (std::size_t index = 0;
             index < model.group_count(); ++index) {
            native::table_group group = model.group(index);
            if (group.id == id)
                return group;
        }
        return std::nullopt;
    }

    native::table_row_id row_id_for_item(native::table_view &table,
                                          int item) {
        native::table_model *model = table.get_model();
        auto &binding = binding_for(table);
        if (!model || item < 0)
            return native::invalid_table_row_id;
        std::size_t model_row = static_cast<std::size_t>(item);
        if (binding.owner_data) {
            if (model_row >= table.get_display_row_count())
                return native::invalid_table_row_id;
            const native::table_display_row display =
                table.get_display_row(model_row);
            if (display.group)
                return native::invalid_table_row_id;
            model_row = display.model_row;
        }
        if (model_row >= model->row_count())
            return native::invalid_table_row_id;
        return model->row_id(model_row);
    }

    native::table_column_id column_for_native(
        const windows::win_table_view &binding,
        int index) {
        return index >= 0 &&
                       static_cast<std::size_t>(index) <
                           binding.native_columns.size()
                   ? binding.native_columns[
                         static_cast<std::size_t>(index)]
                   : 0;
    }

    const native::table_column *table_column_for_native(
        native::table_view &table,
        const windows::win_table_view &binding,
        int index) {
        const native::table_column_id id =
            column_for_native(binding, index);
        const auto found = std::find_if(
            table.get_columns().begin(),
            table.get_columns().end(),
            [id](const native::table_column &column) {
                return column.id == id;
            });
        return found == table.get_columns().end() ? nullptr : &*found;
    }

    bool model_row_for_native_item(
        native::table_view &table,
        const windows::win_table_view &binding,
        int item,
        std::size_t &model_row,
        native::table_row_id &row_id) {
        native::table_model *model = table.get_model();
        if (!model || item < 0)
            return false;
        model_row = static_cast<std::size_t>(item);
        if (binding.owner_data) {
            if (model_row >= table.get_display_row_count())
                return false;
            const native::table_display_row display =
                table.get_display_row(model_row);
            if (display.group)
                return false;
            model_row = display.model_row;
        }
        if (model_row >= model->row_count())
            return false;
        row_id = model->row_id(model_row);
        return true;
    }

    void reset_images(native::table_view &table) {
        auto &binding = binding_for(table);
        if (binding.images)
            ImageList_Destroy(binding.images);
        const native::size dimensions = icon_dimensions(table);
        binding.images = ImageList_Create(dimensions.w,
                                          dimensions.h,
                                          ILC_COLOR32,
                                          8,
                                          8);
        binding.image_indexes.clear();
        ListView_SetImageList(binding.hwnd,
                              binding.images,
                              LVSIL_SMALL);
    }

    void rebuild_columns(native::table_view &table) {
        auto &binding = binding_for(table);
        HWND header = ListView_GetHeader(binding.hwnd);
        int count = header ? Header_GetItemCount(header) : 0;
        while (count-- > 0)
            ListView_DeleteColumn(binding.hwnd, 0);
        binding.native_columns.clear();
        int native_index = 0;
        for (const auto &column : table.get_columns()) {
            if (!column.visible)
                continue;
            std::wstring title = windows::utf8_to_wide(column.title);
            LVCOLUMNW value{};
            value.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT |
                         LVCF_SUBITEM;
            value.pszText = title.data();
            value.cx = column.width;
            value.iSubItem = native_index;
            value.fmt = column.alignment == native::table_alignment::end
                            ? LVCFMT_RIGHT
                            : (column.alignment ==
                                       native::table_alignment::center
                                   ? LVCFMT_CENTER
                                   : LVCFMT_LEFT);
            if (table.get_sort() &&
                table.get_sort()->column == column.id) {
                value.fmt |= table.get_sort()->direction ==
                                     native::sort_direction::ascending
                                 ? HDF_SORTUP
                                 : HDF_SORTDOWN;
            }
            ListView_InsertColumn(binding.hwnd, native_index, &value);
            binding.native_columns.push_back(column.id);
            ++native_index;
        }
        if (header)
            ShowWindow(header,
                       table.get_header_visible() ? SW_SHOW : SW_HIDE);
    }

    void fit_last_column(native::table_view &table) {
        auto &binding = binding_for(table);
        if (!table.get_fill_last_column() ||
            binding.native_columns.empty()) {
            return;
        }

        RECT client{};
        if (!GetClientRect(binding.hwnd, &client))
            return;
        int available = std::max(
            0, static_cast<int>(client.right - client.left));
        if ((GetWindowLongPtrW(binding.hwnd, GWL_STYLE) & WS_VSCROLL) != 0)
            available = std::max(
                0, available - GetSystemMetrics(SM_CXVSCROLL));

        int total = 0;
        int last_width = 0;
        for (native::table_column_id id : binding.native_columns) {
            const auto found = std::find_if(
                table.get_columns().begin(), table.get_columns().end(),
                [id](const native::table_column &column) {
                    return column.id == id;
                });
            if (found == table.get_columns().end())
                continue;
            total += found->width;
            last_width = found->width;
        }
        if (available > total) {
            ListView_SetColumnWidth(
                binding.hwnd,
                static_cast<int>(binding.native_columns.size() - 1),
                last_width + available - total);
        }
    }

    int native_group_for_row(native::table_model &model,
                             std::size_t row) {
        for (std::size_t index = 0;
             index < model.group_count(); ++index) {
            const native::table_group group = model.group(index);
            if (row >= group.first_row &&
                row < group.first_row + group.row_count) {
                return static_cast<int>(index + 1);
            }
        }
        return I_GROUPIDNONE;
    }

    void rebuild_materialized(native::table_view &table) {
        auto &binding = binding_for(table);
        native::table_model *model = table.get_model();
        ListView_DeleteAllItems(binding.hwnd);
        SendMessageW(binding.hwnd, LVM_REMOVEALLGROUPS, 0, 0);
        if (!model)
            return;
        if (model->group_count() > 0) {
            for (std::size_t index = 0;
                 index < model->group_count(); ++index) {
                const native::table_group group = model->group(index);
                std::wstring title = windows::utf8_to_wide(group.title);
                LVGROUP native_group{};
                native_group.cbSize = sizeof(native_group);
                native_group.mask = LVGF_GROUPID | LVGF_HEADER;
                native_group.iGroupId = static_cast<int>(index + 1);
                native_group.pszHeader = title.data();
#ifdef LVGF_STATE
                if (group.collapsible) {
                    native_group.mask |= LVGF_STATE;
                    native_group.stateMask = LVGS_COLLAPSIBLE |
                                             LVGS_COLLAPSED;
                    native_group.state = LVGS_COLLAPSIBLE |
                        (table.get_group_expanded(group.id)
                             ? 0
                             : LVGS_COLLAPSED);
                }
#endif
                ListView_InsertGroup(binding.hwnd,
                                     static_cast<int>(index),
                                     &native_group);
            }
            ListView_EnableGroupView(binding.hwnd, TRUE);
        } else {
            ListView_EnableGroupView(binding.hwnd, FALSE);
        }
        for (std::size_t row = 0; row < model->row_count(); ++row) {
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
            item.iItem = static_cast<int>(row);
            item.iGroupId = native_group_for_row(*model, row);
            item.lParam = static_cast<LPARAM>(model->row_id(row));
            std::wstring first;
            if (!binding.native_columns.empty()) {
                const native::table_cell cell = model->cell(
                    row, binding.native_columns.front());
                first = windows::utf8_to_wide(cell.text);
                item.pszText = first.data();
                if (cell.image) {
                    item.mask |= LVIF_IMAGE;
                    item.iImage = image_index(table, cell.image);
                }
            } else {
                item.pszText = const_cast<wchar_t *>(L"");
            }
            ListView_InsertItem(binding.hwnd, &item);
            for (std::size_t column = 1;
                 column < binding.native_columns.size(); ++column) {
                const native::table_cell cell = model->cell(
                    row, binding.native_columns[column]);
                std::wstring text = windows::utf8_to_wide(cell.text);
                LVITEMW subitem{};
                subitem.mask = LVIF_TEXT;
                subitem.iItem = static_cast<int>(row);
                subitem.iSubItem = static_cast<int>(column);
                subitem.pszText = text.data();
                if (cell.image) {
                    subitem.mask |= LVIF_IMAGE;
                    subitem.iImage = image_index(table, cell.image);
                }
                ListView_SetItem(binding.hwnd, &subitem);
            }
        }
    }

    void rebuild(native::table_view &table) {
        auto &binding = binding_for(table);
        binding.suppress = true;
        rebuild_columns(table);
        reset_images(table);
        DWORD extended = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                         LVS_EX_SUBITEMIMAGES;
        if (table.get_grid_lines() == native::table_grid_lines::both)
            extended |= LVS_EX_GRIDLINES;
        if (table.get_columns_reorderable())
            extended |= LVS_EX_HEADERDRAGDROP;
        ListView_SetExtendedListViewStyle(binding.hwnd, extended);
        if (binding.owner_data) {
            ListView_SetItemCountEx(
                binding.hwnd,
                static_cast<int>(std::min<std::size_t>(
                    table.get_display_row_count(), INT_MAX)),
                LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
        } else {
            rebuild_materialized(table);
        }
        ShowScrollBar(binding.hwnd,
                      SB_VERT,
                      table.get_vertical_scrollbar_policy() !=
                          native::scrollbar_policy::never);
        ShowScrollBar(binding.hwnd,
                      SB_HORZ,
                      table.get_horizontal_scrollbar_policy() !=
                          native::scrollbar_policy::never);
        fit_last_column(table);
        binding.suppress = false;
    }
} // namespace

namespace windows
{
    LRESULT handle_table_notify(native::table_view *table,
                                NMHDR *notification) {
        if (!table || !notification)
            return 0;
        auto &binding = binding_for(*table);
        if (binding.suppress)
            return 0;
        if (notification->code == LVN_GETDISPINFOW) {
            auto *info = reinterpret_cast<NMLVDISPINFOW *>(notification);
            native::table_model *model = table->get_model();
            const int item = info->item.iItem;
            const int subitem = info->item.iSubItem;
            if (!model || item < 0)
                return 0;
            const native::table_display_row display =
                table->get_display_row(static_cast<std::size_t>(item));
            std::wstring text;
            const native::img *image = nullptr;
            if (display.group) {
                const auto group = group_by_id(*model, display.group_id);
                if (subitem == 0 && group)
                    text = utf8_to_wide(group->title);
            } else {
                const native::table_column_id column =
                    column_for_native(binding, subitem);
                if (column != 0) {
                    const native::table_cell cell = model->cell(
                        display.model_row, column);
                    text = utf8_to_wide(cell.text);
                    image = cell.image;
                }
            }
            if ((info->item.mask & LVIF_TEXT) && info->item.pszText &&
                info->item.cchTextMax > 0) {
                wcsncpy_s(info->item.pszText,
                          static_cast<std::size_t>(
                              info->item.cchTextMax),
                          text.c_str(),
                          _TRUNCATE);
            }
            if (info->item.mask & LVIF_IMAGE)
                info->item.iImage = image_index(*table, image);
            return 0;
        }
        if (notification->code == LVN_ITEMCHANGING &&
            binding.owner_data) {
            auto *change = reinterpret_cast<NMLISTVIEW *>(notification);
            if ((change->uNewState & LVIS_SELECTED) != 0 &&
                change->iItem >= 0 &&
                table->get_display_row(
                    static_cast<std::size_t>(change->iItem)).group) {
                return TRUE;
            }
        }
        if (notification->code == LVN_ITEMCHANGED) {
            std::vector<native::table_row_id> rows;
            for (int item = ListView_GetNextItem(
                     binding.hwnd, -1, LVNI_SELECTED);
                 item >= 0;
                 item = ListView_GetNextItem(
                     binding.hwnd, item, LVNI_SELECTED)) {
                const native::table_row_id id =
                    row_id_for_item(*table, item);
                if (id != native::invalid_table_row_id)
                    rows.push_back(id);
            }
            table->on_native_selection(rows);
            return 0;
        }
        if (notification->code == NM_CLICK && binding.owner_data) {
            auto *click = reinterpret_cast<NMITEMACTIVATE *>(notification);
            if (click->iItem < 0)
                return 0;
            const native::table_display_row display =
                table->get_display_row(
                    static_cast<std::size_t>(click->iItem));
            if (!display.group)
                return 0;
            native::table_model *model = table->get_model();
            const auto group = model
                ? group_by_id(*model, display.group_id)
                : std::nullopt;
            if (group && group->collapsible) {
                table->on_native_group_expand(
                    display.group_id,
                    !table->get_group_expanded(display.group_id));
            }
            return 0;
        }
        if (notification->code == NM_DBLCLK ||
            notification->code == NM_RETURN) {
            const auto rows = table->get_selected_rows();
            if (!rows.empty())
                table->on_native_activate(rows.back());
            return 0;
        }
        if (notification->code == LVN_COLUMNCLICK) {
            auto *click = reinterpret_cast<NMLISTVIEW *>(notification);
            table->on_native_sort_request(
                column_for_native(binding, click->iSubItem));
            return 0;
        }
        if (notification->code == LVN_ODFINDITEMW) {
            auto *find = reinterpret_cast<NMLVFINDITEMW *>(notification);
            if (!find->lvfi.psz || !table->get_type_search_enabled())
                return -1;
            native::table_search query;
            query.text = wide_to_utf8(find->lvfi.psz);
            query.match = native::table_search_match::prefix;
            query.start_row = find->iStart >= 0
                                  ? static_cast<std::size_t>(find->iStart)
                                  : 0;
            if (!table->find_and_reveal(query))
                return -1;
            return ListView_GetNextItem(
                binding.hwnd, -1, LVNI_SELECTED);
        }
        if (notification->code == NM_CUSTOMDRAW) {
            auto *native_draw = reinterpret_cast<NMCUSTOMDRAW *>(
                notification);
            windows::scoped_gpx_dc custom_draw_context(
                table->get_gpx(), native_draw->hdc);
            HWND header = ListView_GetHeader(binding.hwnd);
            if (notification->hwndFrom == header) {
                auto *draw = reinterpret_cast<NMCUSTOMDRAW *>(
                    notification);
                if (draw->dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                    const int native_column =
                        static_cast<int>(draw->dwItemSpec);
                    const native::table_column *column =
                        table_column_for_native(
                            *table, binding, native_column);
                    RECT item_bounds{};
                    if (!column ||
                        !Header_GetItemRect(
                            header, native_column, &item_bounds)) {
                        return CDRF_DODEFAULT;
                    }
                    native::rect bounds(
                        static_cast<native::coord>(item_bounds.left),
                        static_cast<native::coord>(item_bounds.top),
                        static_cast<native::dim>(
                            item_bounds.right - item_bounds.left),
                        static_cast<native::dim>(
                            item_bounds.bottom - item_bounds.top));
                    native::gpx &graphics =
                        table->get_gpx().set_clip(bounds);
                    auto appearance = native::theme::create(graphics);
                    native::theme::state state;
                    state.hot = (draw->uItemState & CDIS_HOT) != 0;
                    state.pressed =
                        (draw->uItemState & CDIS_SELECTED) != 0;
                    native::detail::control_render_access::
                        draw_table_header(
                            *table,
                            graphics,
                            *appearance,
                            *column,
                            bounds,
                            state);
                    return CDRF_SKIPDEFAULT;
                }
                return CDRF_DODEFAULT;
            }
            auto *draw = reinterpret_cast<NMLVCUSTOMDRAW *>(notification);
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const int item = static_cast<int>(draw->nmcd.dwItemSpec);
                if (binding.owner_data && item >= 0) {
                    const native::table_display_row display =
                        table->get_display_row(
                            static_cast<std::size_t>(item));
                    if (display.group) {
                        RECT bounds{};
                        ListView_GetItemRect(binding.hwnd,
                                             item,
                                             &bounds,
                                             LVIR_BOUNDS);
                        native::table_model *model = table->get_model();
                        const auto group = model
                            ? group_by_id(*model, display.group_id)
                            : std::nullopt;
                        if (group) {
                            native::rect native_bounds(
                                bounds.left,
                                bounds.top,
                                bounds.right - bounds.left,
                                bounds.bottom - bounds.top);
                            native::gpx &graphics =
                                table->get_gpx().set_clip(native_bounds);
                            auto appearance =
                                native::theme::create(graphics);
                            native::theme::state state;
                            native::detail::control_render_access::
                                draw_table_group(
                                    *table,
                                    graphics,
                                    *appearance,
                                    *group,
                                    native_bounds,
                                    state);
                        }
                        return CDRF_SKIPDEFAULT;
                    }
                }
                return CDRF_NOTIFYSUBITEMDRAW |
                       CDRF_NOTIFYPOSTPAINT;
            }
            if (draw->nmcd.dwDrawStage ==
                (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                const int item =
                    static_cast<int>(draw->nmcd.dwItemSpec);
                const int subitem = draw->iSubItem;
                std::size_t model_row = 0;
                native::table_row_id row_id =
                    native::invalid_table_row_id;
                const native::table_column *column =
                    table_column_for_native(*table, binding, subitem);
                native::table_model *model = table->get_model();
                if (!column || !model ||
                    !model_row_for_native_item(
                        *table,
                        binding,
                        item,
                        model_row,
                        row_id)) {
                    return CDRF_DODEFAULT;
                }
                RECT cell{};
                if (!ListView_GetSubItemRect(
                        binding.hwnd,
                        item,
                        subitem,
                        LVIR_BOUNDS,
                        &cell)) {
                    return CDRF_DODEFAULT;
                }
                native::rect bounds(
                    cell.left,
                    cell.top,
                    cell.right - cell.left,
                    cell.bottom - cell.top);
                native::gpx &graphics =
                    table->get_gpx().set_clip(bounds);
                auto appearance = native::theme::create(graphics);
                native::theme::state state;
                state.selected =
                    (draw->nmcd.uItemState & CDIS_SELECTED) != 0;
                state.focused =
                    (draw->nmcd.uItemState & CDIS_FOCUS) != 0;
                state.hot =
                    (draw->nmcd.uItemState & CDIS_HOT) != 0;
                state.disabled =
                    (draw->nmcd.uItemState & CDIS_DISABLED) != 0;
                if (subitem == 0) {
                    RECT row{};
                    if (ListView_GetItemRect(
                            binding.hwnd, item, &row, LVIR_BOUNDS)) {
                        native::detail::control_render_access::
                            draw_table_row_background(
                                *table,
                                graphics,
                                *appearance,
                                row_id,
                                model_row,
                                native::rect(
                                    row.left,
                                    row.top,
                                    row.right - row.left,
                                    row.bottom - row.top),
                                state);
                    }
                }
                const native::table_cell value =
                    model->cell(model_row, column->id);
                native::detail::control_render_access::draw_table_cell(
                    *table,
                    graphics,
                    *appearance,
                    row_id,
                    model_row,
                    *column,
                    value,
                    bounds,
                    state);
                return CDRF_SKIPDEFAULT;
            }
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT) {
                const int item =
                    static_cast<int>(draw->nmcd.dwItemSpec);
                std::size_t model_row = 0;
                native::table_row_id row_id =
                    native::invalid_table_row_id;
                if (!model_row_for_native_item(
                        *table,
                        binding,
                        item,
                        model_row,
                        row_id)) {
                    return CDRF_DODEFAULT;
                }
                RECT row{};
                if (ListView_GetItemRect(
                        binding.hwnd, item, &row, LVIR_BOUNDS)) {
                    native::rect bounds(
                        row.left,
                        row.top,
                        row.right - row.left,
                        row.bottom - row.top);
                    native::gpx &graphics =
                        table->get_gpx().set_clip(bounds);
                    auto appearance = native::theme::create(graphics);
                    native::theme::state state;
                    state.selected =
                        (draw->nmcd.uItemState & CDIS_SELECTED) != 0;
                    state.focused =
                        (draw->nmcd.uItemState & CDIS_FOCUS) != 0;
                    native::detail::control_render_access::
                        draw_table_row_focus(
                            *table,
                            graphics,
                            *appearance,
                            row_id,
                            model_row,
                            bounds,
                            state);
                }
                return CDRF_DODEFAULT;
            }
        }
        if (notification->code == HDN_ENDTRACKW ||
            notification->code == HDN_ENDTRACKA) {
            auto *header = reinterpret_cast<NMHEADER *>(notification);
            const native::table_column_id column =
                column_for_native(binding, header->iItem);
            if (column && header->pitem &&
                (header->pitem->mask & HDI_WIDTH)) {
                table->on_native_column_resize(
                    column,
                    static_cast<native::dim>(std::clamp(
                        header->pitem->cxy, 0, 65535)));
            }
            return 0;
        }
        if (notification->code == HDN_ENDDRAG) {
            auto *header = reinterpret_cast<NMHEADER *>(notification);
            if (header->pitem &&
                (header->pitem->mask & HDI_ORDER)) {
                table->on_native_column_move(
                    column_for_native(binding, header->iItem),
                    static_cast<std::size_t>(
                        std::max(0, header->pitem->iOrder)));
            }
            return 0;
        }
        return 0;
    }
} // namespace windows

namespace native
{
    void table_view::apply_table() { rebuild(*this); }

    void table_view::apply_selection() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        ListView_SetItemState(binding.hwnd,
                              -1,
                              0,
                              LVIS_SELECTED | LVIS_FOCUSED);
        for (table_row_id id : _selection) {
            const auto model_row =
                id == _focused_row && _focused_model_row
                    ? _focused_model_row
                    : model_row_for_id(id);
            if (!model_row)
                continue;
            std::optional<std::size_t> item = *model_row;
            if (binding.owner_data)
                item = _visible_rows->display_index_for_model_row(
                    *model_row);
            if (item && *item <= static_cast<std::size_t>(INT_MAX)) {
                ListView_SetItemState(
                    binding.hwnd,
                    static_cast<int>(*item),
                    LVIS_SELECTED | LVIS_FOCUSED,
                    LVIS_SELECTED | LVIS_FOCUSED);
            }
        }
        binding.suppress = false;
    }

    void table_view::apply_scroll() {
        auto &binding = binding_for(*this);
        if (_vertical_row < get_display_row_count()) {
            ListView_EnsureVisible(binding.hwnd,
                                   static_cast<int>(_vertical_row),
                                   FALSE);
        }
        const int delta = _horizontal_offset -
                          binding.horizontal_offset;
        if (delta != 0) {
            ListView_Scroll(binding.hwnd, delta, 0);
            binding.horizontal_offset = _horizontal_offset;
        }
    }

    void table_view::create_native() {
        wnd *parent = get_parent();
        HWND parent_hwnd = parent
            ? windows::wnd_bindings.handle_from_object(parent)
            : nullptr;
        if (!parent || !parent->get_created() || !parent_hwnd)
            throw std::runtime_error(
                "Windows: table_view requires a created parent.");
        INITCOMMONCONTROLSEX controls{sizeof(controls),
                                      ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        auto *self = this;
        const bool owner_data =
            _data_mode != table_data_mode::materialized;
        DWORD style = WS_CHILD | WS_TABSTOP | WS_BORDER |
                      LVS_REPORT | LVS_SHOWSELALWAYS;
        if (_selection_mode == table_selection_mode::single)
            style |= LVS_SINGLESEL;
        if (owner_data)
            style |= LVS_OWNERDATA;
        HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE,
                                    WC_LISTVIEWW,
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
                "Windows: failed to create report ListView.");
        windows::wnd_bindings.register_pair(hwnd, self);
        auto *binding = new windows::win_table_view();
        binding->hwnd = hwnd;
        binding->owner_data = owner_data;
        windows::table_view_bindings.register_pair(self, binding);
        SendMessageW(hwnd,
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(windows::control_font()),
                     TRUE);
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_selection();
        self->apply_scroll();
    }

    void table_view::show_native() {
        auto *binding = windows::table_view_bindings
                            .object_from_handle(
                                this);
        if (!_created || !binding || !binding->hwnd)
            throw std::runtime_error(
                "Windows: table_view is not created.");
        ShowWindow(binding->hwnd, SW_SHOW);
        UpdateWindow(binding->hwnd);
    }

    void table_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            windows::table_view_bindings.object_from_handle(self);
        if (binding) {
            if (binding->images)
                ImageList_Destroy(binding->images);
            if (binding->hwnd) {
                DestroyWindow(binding->hwnd);
                windows::wnd_bindings.unregister_by_handle(
                    binding->hwnd);
            }
            windows::table_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
