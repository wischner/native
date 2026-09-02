//
// Implements table_view with XmContainer detail view for explicitly
// materialized data and the compact Motif-painted virtual fallback.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <Xm/Container.h>
#include <Xm/IconG.h>
#include <Xm/ScrolledW.h>
#include <Xm/Xm.h>

#include <native.h>

#include "../../platforms/linux/x_image.h"
#include "collection_host.h"
#include "globals.h"

namespace
{
    using collection_state = linux::openmotif::motif_collection;

    collection_state &binding_for(native::table_view &table) {
        auto *state = linux::openmotif::table_view_bindings
                          .object_from_handle(&table);
        if (!state || !state->widget)
            throw std::runtime_error(
                "Motif: missing table_view binding.");
        return *state;
    }

    std::vector<native::table_column> visible_columns(
        const native::table_view &table) {
        std::vector<native::table_column> result;
        for (const auto &column : table.get_columns()) {
            if (column.visible)
                result.push_back(column);
        }
        return result;
    }

    std::optional<native::table_group> group_for_row(
        native::table_model &model,
        std::size_t row,
        std::size_t *group_index = nullptr) {
        for (std::size_t index = 0;
             index < model.group_count(); ++index) {
            const native::table_group group = model.group(index);
            if (row >= group.first_row &&
                row < group.first_row + group.row_count) {
                if (group_index)
                    *group_index = index;
                return group;
            }
        }
        return std::nullopt;
    }

    void free_pixmaps(collection_state &state) {
        for (Pixmap pixmap : state.pixmaps) {
            if (pixmap != XmUNSPECIFIED_PIXMAP) {
                XFreePixmap(linux::openmotif::cached_display,
                            pixmap);
            }
        }
        state.pixmaps.clear();
    }

    void clear_items(collection_state &state) {
        for (Widget item : state.items)
            XtDestroyWidget(item);
        for (Widget item : state.group_items)
            XtDestroyWidget(item);
        state.items.clear();
        state.row_ids.clear();
        state.group_items.clear();
        state.group_ids.clear();
        free_pixmaps(state);
    }

    Pixmap create_icon(collection_state &state,
                       const native::img *image) {
        if (!image || image->w() <= 0 || image->h() <= 0)
            return XmUNSPECIFIED_PIXMAP;
        Display *display = linux::openmotif::cached_display;
        Screen *screen = XtScreen(state.content);
        Pixmap pixmap = XCreatePixmap(
            display,
            RootWindowOfScreen(screen),
            static_cast<unsigned>(image->w()),
            static_cast<unsigned>(image->h()),
            static_cast<unsigned>(DefaultDepthOfScreen(screen)));
        if (pixmap == None)
            return XmUNSPECIFIED_PIXMAP;
        Pixel background = 0;
        XtVaGetValues(state.content,
                      XmNbackground,
                      &background,
                      nullptr);
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XSetForeground(display, gc, background);
        XFillRectangle(display,
                       pixmap,
                       gc,
                       0,
                       0,
                       static_cast<unsigned>(image->w()),
                       static_cast<unsigned>(image->h()));
        native::detail::blend_x_image(
            display,
            pixmap,
            gc,
            *image,
            {0, 0},
            native::rect(0, 0, image->w(), image->h()),
            {static_cast<native::dim>(image->w()),
             static_cast<native::dim>(image->h())});
        XFreeGC(display, gc);
        state.pixmaps.push_back(pixmap);
        return pixmap;
    }

    Widget group_widget(collection_state &state,
                        native::table_group_id id) {
        const auto found = std::find(state.group_ids.begin(),
                                     state.group_ids.end(),
                                     id);
        if (found == state.group_ids.end())
            return nullptr;
        return state.group_items[
            static_cast<std::size_t>(found -
                                     state.group_ids.begin())];
    }

    Widget row_widget(collection_state &state,
                      native::table_row_id id) {
        const auto found = std::find(state.row_ids.begin(),
                                     state.row_ids.end(),
                                     id);
        if (found == state.row_ids.end())
            return nullptr;
        return state.items[
            static_cast<std::size_t>(found - state.row_ids.begin())];
    }

    void selection_changed(Widget,
                           XtPointer client_data,
                           XtPointer call_data) {
        auto *table = static_cast<native::table_view *>(client_data);
        if (!table || !call_data)
            return;
        auto &state = binding_for(*table);
        if (state.suppress)
            return;
        auto *selected =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        std::vector<native::table_row_id> rows;
        for (int item = 0;
             item < selected->selected_item_count;
             ++item) {
            const auto found = std::find(
                state.items.begin(),
                state.items.end(),
                selected->selected_items[item]);
            if (found != state.items.end()) {
                rows.push_back(state.row_ids[
                    static_cast<std::size_t>(found -
                                             state.items.begin())]);
            }
        }
        table->on_native_selection(rows);
    }

    void default_action(Widget,
                        XtPointer client_data,
                        XtPointer call_data) {
        auto *table = static_cast<native::table_view *>(client_data);
        if (!table || !call_data)
            return;
        auto &state = binding_for(*table);
        auto *selected =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        for (int item = 0;
             item < selected->selected_item_count;
             ++item) {
            const auto found = std::find(
                state.items.begin(),
                state.items.end(),
                selected->selected_items[item]);
            if (found != state.items.end()) {
                table->on_native_activate(state.row_ids[
                    static_cast<std::size_t>(found -
                                             state.items.begin())]);
                return;
            }
        }
    }

    void outline_changed(Widget,
                         XtPointer client_data,
                         XtPointer call_data) {
        auto *table = static_cast<native::table_view *>(client_data);
        if (!table || !call_data)
            return;
        auto &state = binding_for(*table);
        if (state.suppress)
            return;
        auto *outline =
            static_cast<XmContainerOutlineCallbackStruct *>(call_data);
        const auto found = std::find(state.group_items.begin(),
                                     state.group_items.end(),
                                     outline->item);
        if (found == state.group_items.end())
            return;
        const std::size_t index = static_cast<std::size_t>(
            found - state.group_items.begin());
        native::table_model *model = table->get_model();
        const native::table_group group = model->group(index);
        if (!group.collapsible) {
            state.suppress = true;
            XtVaSetValues(outline->item,
                          XmNoutlineState,
                          XmEXPANDED,
                          nullptr);
            state.suppress = false;
            return;
        }
        table->on_native_group_expand(
            state.group_ids[index],
            outline->new_outline_state == XmEXPANDED);
    }

    void update_scrollbars(native::table_view &table,
                           collection_state &state) {
        Widget horizontal = nullptr;
        Widget vertical = nullptr;
        XtVaGetValues(state.widget,
                      XmNhorizontalScrollBar,
                      &horizontal,
                      XmNverticalScrollBar,
                      &vertical,
                      nullptr);
        if (horizontal && table.get_horizontal_scrollbar_policy() ==
                              native::scrollbar_policy::never) {
            XtUnmanageChild(horizontal);
        }
        if (vertical && table.get_vertical_scrollbar_policy() ==
                            native::scrollbar_policy::never) {
            XtUnmanageChild(vertical);
        }
    }

    void rebuild_native(native::table_view &table) {
        auto &state = binding_for(table);
        if (!state.native_table || !state.content)
            return;
        state.suppress = true;
        clear_items(state);
        native::table_model *model = table.get_model();
        const auto columns = visible_columns(table);

        std::vector<XmString> headings;
        if (table.get_header_visible()) {
            headings.reserve(columns.size());
            for (const auto &column : columns) {
                headings.push_back(XmStringCreateLocalized(
                    const_cast<char *>(column.title.c_str())));
            }
        }
        XtVaSetValues(
            state.content,
            XmNdetailColumnHeading,
            headings.empty() ? nullptr : headings.data(),
            XmNdetailColumnHeadingCount,
            static_cast<Cardinal>(headings.size()),
            XmNdetailOrderCount,
            static_cast<Cardinal>(columns.empty()
                                      ? 0
                                      : columns.size() - 1),
            XmNselectionPolicy,
            table.get_selection_mode() ==
                    native::table_selection_mode::single
                ? XmSINGLE_SELECT
                : XmEXTENDED_SELECT,
            nullptr);
        for (XmString heading : headings)
            XmStringFree(heading);
        if (!model) {
            state.suppress = false;
            return;
        }

        for (std::size_t index = 0;
             index < model->group_count(); ++index) {
            const native::table_group group = model->group(index);
            XmString label = XmStringCreateLocalized(
                const_cast<char *>(group.title.c_str()));
            Widget item = XmVaCreateManagedIconGadget(
                state.content,
                const_cast<char *>("group"),
                XmNlabelString,
                label,
                XmNviewType,
                XmSMALL_ICON,
                XmNoutlineState,
                table.get_group_expanded(group.id)
                    ? XmEXPANDED
                    : XmCOLLAPSED,
                XmNpositionIndex,
                static_cast<int>(group.first_row * 2),
                nullptr);
            XmStringFree(label);
            state.group_items.push_back(item);
            state.group_ids.push_back(group.id);
        }

        for (std::size_t row = 0; row < model->row_count(); ++row) {
            std::size_t group_index = 0;
            const auto group = group_for_row(
                *model, row, &group_index);
            const native::table_cell primary = columns.empty()
                ? native::table_cell{}
                : model->cell(row, columns.front().id);
            XmString label = XmStringCreateLocalized(
                const_cast<char *>(primary.text.c_str()));
            std::vector<XmString> details;
            for (std::size_t column = 1;
                 column < columns.size(); ++column) {
                const native::table_cell value = model->cell(
                    row, columns[column].id);
                details.push_back(XmStringCreateLocalized(
                    const_cast<char *>(value.text.c_str())));
            }
            const Pixmap pixmap = create_icon(state, primary.image);
            Widget item = XmVaCreateManagedIconGadget(
                state.content,
                const_cast<char *>("row"),
                XmNlabelString,
                label,
                XmNdetail,
                details.empty() ? nullptr : details.data(),
                XmNdetailCount,
                static_cast<Cardinal>(details.size()),
                XmNviewType,
                XmSMALL_ICON,
                XmNsmallIconPixmap,
                pixmap,
                XmNentryParent,
                group ? state.group_items[group_index] : nullptr,
                XmNpositionIndex,
                group
                    ? static_cast<int>(row - group->first_row)
                    : static_cast<int>(row * 2 + 1),
                nullptr);
            XmStringFree(label);
            for (XmString detail : details)
                XmStringFree(detail);
            state.items.push_back(item);
            state.row_ids.push_back(model->row_id(row));
        }
        XmContainerRelayout(state.content);
        state.suppress = false;
        update_scrollbars(table, state);
    }

    Widget create_native_table(native::table_view &table,
                               collection_state &state) {
        native::wnd *parent = table.get_parent();
        Widget parent_widget = linux::openmotif::parent_widget(&table);
        if (!parent || !parent->get_created() || !parent_widget) {
            throw std::runtime_error(
                "Motif: table_view requires a created parent.");
        }
        const native::rect bounds = table.get_bounds();
        Widget scroller = XtVaCreateWidget(
            "tableScroll",
            xmScrolledWindowWidgetClass,
            parent_widget,
            XmNx,
            bounds.p.x,
            XmNy,
            bounds.p.y,
            XmNwidth,
            bounds.d.w,
            XmNheight,
            bounds.d.h,
            XmNscrollingPolicy,
            XmAUTOMATIC,
            XmNscrollBarDisplayPolicy,
            table.get_horizontal_scrollbar_policy() ==
                        native::scrollbar_policy::always ||
                    table.get_vertical_scrollbar_policy() ==
                        native::scrollbar_policy::always
                ? XmSTATIC
                : XmDYNAMIC,
            nullptr);
        state.content = XmVaCreateManagedContainer(
            scroller,
            const_cast<char *>("table"),
            XmNlayoutType,
            XmDETAIL,
            XmNentryViewType,
            XmSMALL_ICON,
            XmNoutlineButtonPolicy,
            XmOUTLINE_BUTTON_PRESENT,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
        XtAddCallback(state.content,
                      XmNselectionCallback,
                      selection_changed,
                      &table);
        XtAddCallback(state.content,
                      XmNdefaultActionCallback,
                      default_action,
                      &table);
        XtAddCallback(state.content,
                      XmNoutlineChangedCallback,
                      outline_changed,
                      &table);
        linux::openmotif::wnd_bindings.register_pair(scroller, &table);
        return scroller;
    }
} // namespace

namespace native
{
    void table_view::apply_table() {
        auto &state = binding_for(*this);
        if (state.native_table)
            rebuild_native(*this);
        else
            invalidate();
    }

    void table_view::apply_selection() {
        auto &state = binding_for(*this);
        if (!state.native_table) {
            invalidate();
            return;
        }
        state.suppress = true;
        for (std::size_t index = 0;
             index < state.items.size(); ++index) {
            const bool selected = std::find(
                _selection.begin(),
                _selection.end(),
                state.row_ids[index]) != _selection.end();
            XtVaSetValues(state.items[index],
                          XmNvisualEmphasis,
                          selected ? XmSELECTED : XmNOT_SELECTED,
                          nullptr);
        }
        state.suppress = false;
    }

    void table_view::apply_scroll() {
        auto &state = binding_for(*this);
        if (!state.native_table ||
            _vertical_row >= get_display_row_count()) {
            invalidate();
            return;
        }
        const table_display_row display =
            get_display_row(_vertical_row);
        Widget item = nullptr;
        if (display.group) {
            item = group_widget(state, display.group_id);
        } else if (_model) {
            item = row_widget(state,
                              _model->row_id(display.model_row));
        }
        if (item)
            XmScrollVisible(state.widget, item, 0, 0);
    }

    void table_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *state = new collection_state();
        state->native_table =
            _data_mode == table_data_mode::materialized;
        if (state->native_table)
            state->widget = create_native_table(*self, *state);
        else
            state->widget =
                linux::openmotif::create_collection_host(*self, *state);
        linux::openmotif::table_view_bindings.register_pair(
            self, state);
        _created = true;
        self->synchronize_theme_metrics();
        if (state->native_table) {
            rebuild_native(*self);
            self->apply_selection();
            self->apply_scroll();
        }
        self->on_native_create();
    }

    void table_view::show() const {
        auto *state = linux::openmotif::table_view_bindings
                          .object_from_handle(
                              const_cast<table_view *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error(
                "Motif: table_view is not created.");
        XtManageChild(state->widget);
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *state = linux::openmotif::table_view_bindings
                          .object_from_handle(self);
        if (state && state->native_table) {
            self->on_native_destroy();
            clear_items(*state);
            if (state->widget) {
                linux::openmotif::wnd_bindings.unregister_by_handle(
                    state->widget);
                XtDestroyWidget(state->widget);
            }
            linux::openmotif::table_view_bindings
                .unregister_by_handle(self);
            delete state;
            return;
        }
        linux::openmotif::destroy_collection_host(*self, state);
        linux::openmotif::table_view_bindings.unregister_by_handle(
            self);
    }
} // namespace native
