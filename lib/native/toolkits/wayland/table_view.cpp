//
// Presents lazy portable display rows in GTK's native virtual column
// view. Stable selection IDs remain owned by Native rather than by GTK
// row indices.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "index_model.h"
#include "../../table_visible_rows.h"
#include <algorithm>
#include <typeinfo>
namespace
{
    using namespace native;
    using namespace linux::wayland;
    struct column_binding
    {
        table_view *owner;
        table_column column;
    };
    void release_binding(gpointer value, GClosure *) {
        delete static_cast<column_binding *>(value);
    }
    GtkColumnView *view(table_view &owner) {
        return GTK_COLUMN_VIEW(state(owner).editor);
    }
    void selection_changed(GtkSelectionModel *selection,
                           guint,
                           guint,
                           gpointer data) {
        auto &owner = *static_cast<table_view *>(data);
        if (state(owner).updating || !owner.get_model())
            return;
        std::vector<table_row_id> ids;
        auto *selected = gtk_selection_model_get_selection(selection);
        GtkBitsetIter iterator;
        guint position = 0;
        if (gtk_bitset_iter_init_first(&iterator, selected, &position))
            do {
                if (position < owner.get_display_row_count()) {
                    auto row = owner.get_display_row(position);
                    if (!row.group)
                        ids.push_back(
                            owner.get_model()->row_id(row.model_row));
                }
            } while (gtk_bitset_iter_next(&iterator, &position));
        gtk_bitset_unref(selected);
        owner.on_native_selection(ids);
    }
} // namespace
namespace native
{
    void table_view::create_native() {
        auto *columns = gtk_column_view_new(nullptr);
        auto *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      columns);
        auto *overlay = gtk_overlay_new();
        auto *drawing = gtk_drawing_area_new();
        gtk_widget_set_hexpand(drawing, true);
        gtk_widget_set_vexpand(drawing, true);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), scroll);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);
        linux::wayland::attach(*this, overlay);
        state(*this).editor = columns;
        state(*this).drawing = drawing;
        state(*this).box = scroll;
        linux::wayland::configure_drawing(*this);
        synchronize_theme_metrics();
        apply_table();
        auto *keys = gtk_event_controller_key_new();
        gtk_event_controller_set_propagation_phase(keys,
                                                   GTK_PHASE_CAPTURE);
        g_signal_connect(
            keys,
            "key-pressed",
            G_CALLBACK(+[](GtkEventControllerKey *,
                           guint key,
                           guint,
                           GdkModifierType modifiers,
                           gpointer data) -> gboolean {
                return linux::wayland::table_key(
                    *static_cast<table_view *>(data), key, modifiers);
            }),
            this);
        gtk_widget_add_controller(columns, keys);
        g_signal_connect(
            gtk_column_view_get_columns(GTK_COLUMN_VIEW(columns)),
            "items-changed",
            G_CALLBACK(+[](GListModel *columns,
                           guint,
                           guint,
                           guint,
                           gpointer data) {
                auto &owner = *static_cast<table_view *>(data);
                if (state(owner).updating)
                    return;
                std::vector<table_column_id> order;
                for (guint i = 0; i < g_list_model_get_n_items(columns);
                     ++i) {
                    auto *column =
                        G_OBJECT(g_list_model_get_item(columns, i));
                    order.push_back(GPOINTER_TO_UINT(
                        g_object_get_data(column, "native-id")));
                    g_object_unref(column);
                }
                std::size_t visible = 0;
                const auto portable = owner.get_columns();
                for (std::size_t i = 0;
                     i < portable.size() && visible < order.size();
                     ++i) {
                    if (!portable[i].visible)
                        continue;
                    if (portable[i].id != order[visible]) {
                        owner.on_native_column_move(order[visible], i);
                        return;
                    }
                    ++visible;
                }
            }),
            this);
        g_signal_connect(
            gtk_column_view_get_sorter(GTK_COLUMN_VIEW(columns)),
            "changed",
            G_CALLBACK(
                +[](GtkSorter *sorter, GtkSorterChange, gpointer data) {
                    auto &owner = *static_cast<table_view *>(data);
                    if (state(owner).updating)
                        return;
                    auto *column =
                        gtk_column_view_sorter_get_primary_sort_column(
                            GTK_COLUMN_VIEW_SORTER(sorter));
                    if (column)
                        owner.on_native_sort_request(
                            GPOINTER_TO_UINT(g_object_get_data(
                                G_OBJECT(column), "native-id")));
                }),
            this);
        g_signal_connect(
            columns,
            "activate",
            G_CALLBACK(
                +[](GtkColumnView *, guint position, gpointer data) {
                    auto &owner = *static_cast<table_view *>(data);
                    if (position >= owner.get_display_row_count() ||
                        !owner.get_model())
                        return;
                    const auto row = owner.get_display_row(position);
                    if (row.group)
                        owner.on_native_group_expand(
                            row.group_id,
                            !owner.get_group_expanded(row.group_id));
                    else
                        owner.on_native_activate(
                            owner.get_model()->row_id(row.model_row));
                }),
            this);
    }
    void table_view::apply_table() {
        auto &peer = state(*this);
        peer.portable_drawing =
            typeid(*this) != typeid(table_view) ||
            !get_header_visible() || get_alternating_rows() ||
            (get_model() && get_model()->group_count() != 0);
        gtk_widget_set_visible(peer.drawing, peer.portable_drawing);
        gtk_widget_set_visible(peer.box, !peer.portable_drawing);
        if (peer.portable_drawing) {
            gtk_widget_queue_draw(peer.drawing);
            return;
        }
        peer.updating = true;
        auto *columns = gtk_column_view_get_columns(view(*this));
        while (g_list_model_get_n_items(columns)) {
            auto *column = GTK_COLUMN_VIEW_COLUMN(
                g_list_model_get_item(columns, 0));
            gtk_column_view_remove_column(view(*this), column);
            g_object_unref(column);
        }
        auto *model = index_model(get_display_row_count());
        GtkSelectionModel *selection =
            get_selection_mode() == table_selection_mode::multiple
                ? GTK_SELECTION_MODEL(gtk_multi_selection_new(model))
                : GTK_SELECTION_MODEL(gtk_single_selection_new(model));
        if (GTK_IS_SINGLE_SELECTION(selection)) {
            gtk_single_selection_set_autoselect(
                GTK_SINGLE_SELECTION(selection), false);
            gtk_single_selection_set_can_unselect(
                GTK_SINGLE_SELECTION(selection), true);
        }
        gtk_column_view_set_model(view(*this), selection);
        g_signal_connect(selection,
                         "selection-changed",
                         G_CALLBACK(selection_changed),
                         this);
        g_object_unref(selection);
        gtk_column_view_set_reorderable(view(*this),
                                        get_columns_reorderable());
        gtk_column_view_set_show_row_separators(
            view(*this),
            get_grid_lines() == table_grid_lines::horizontal ||
                get_grid_lines() == table_grid_lines::both);
        gtk_column_view_set_show_column_separators(
            view(*this),
            get_grid_lines() == table_grid_lines::vertical ||
                get_grid_lines() == table_grid_lines::both);
        auto *header_menu = g_menu_new();
        auto *actions = g_simple_action_group_new();
        if (get_column_visibility_menu_enabled())
            for (const auto &column : get_columns()) {
                const auto name = "column" + std::to_string(column.id);
                auto *action = g_simple_action_new_stateful(
                    name.c_str(),
                    nullptr,
                    g_variant_new_boolean(column.visible));
                g_object_set_data(G_OBJECT(action),
                                  "native-id",
                                  GUINT_TO_POINTER(column.id));
                g_signal_connect(
                    action,
                    "activate",
                    G_CALLBACK(+[](GSimpleAction *action,
                                   GVariant *,
                                   gpointer data) {
                        auto &owner = *static_cast<table_view *>(data);
                        auto *value =
                            g_action_get_state(G_ACTION(action));
                        const bool visible =
                            g_variant_get_boolean(value);
                        g_variant_unref(value);
                        owner.set_column_visible(
                            GPOINTER_TO_UINT(g_object_get_data(
                                G_OBJECT(action), "native-id")),
                            !visible);
                    }),
                    this);
                g_action_map_add_action(G_ACTION_MAP(actions),
                                        G_ACTION(action));
                g_object_unref(action);
                g_menu_append(header_menu,
                              column.title.c_str(),
                              ("columns." + name).c_str());
            }
        gtk_widget_insert_action_group(
            peer.widget, "columns", G_ACTION_GROUP(actions));
        g_object_unref(actions);
        for (const auto &column : get_columns()) {
            if (!column.visible)
                continue;
            auto *factory = gtk_signal_list_item_factory_new();
            g_signal_connect(
                factory,
                "setup",
                G_CALLBACK(+[](GtkSignalListItemFactory *,
                               GtkListItem *item,
                               gpointer) {
                    auto *box =
                        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                    gtk_box_append(GTK_BOX(box), gtk_image_new());
                    auto *label = gtk_label_new(nullptr);
                    gtk_label_set_ellipsize(GTK_LABEL(label),
                                            PANGO_ELLIPSIZE_END);
                    gtk_box_append(GTK_BOX(box), label);
                    gtk_list_item_set_child(item, box);
                }),
                nullptr);
            g_signal_connect_data(
                factory,
                "bind",
                G_CALLBACK(+[](GtkSignalListItemFactory *,
                               GtkListItem *item,
                               gpointer data) {
                    auto &binding =
                        *static_cast<column_binding *>(data);
                    auto &owner = *binding.owner;
                    const auto position = item_index(
                        G_OBJECT(gtk_list_item_get_item(item)));
                    if (position >= owner.get_display_row_count() ||
                        !owner.get_model())
                        return;
                    const auto row = owner.get_display_row(position);
                    std::string text;
                    const img *image = nullptr;
                    if (row.group) {
                        for (std::size_t i = 0;
                             i < owner.get_model()->group_count();
                             ++i) {
                            auto group = owner.get_model()->group(i);
                            if (group.id == row.group_id) {
                                text = group.title;
                                break;
                            }
                        }
                    } else {
                        auto cell = owner.get_model()->cell(
                            row.model_row, binding.column.id);
                        text = std::move(cell.text);
                        image = cell.image;
                    }
                    auto *box = gtk_list_item_get_child(item);
                    auto *picture = gtk_widget_get_first_child(box);
                    auto *label = gtk_widget_get_next_sibling(picture);
                    gtk_label_set_text(GTK_LABEL(label), text.c_str());
                    gtk_label_set_xalign(GTK_LABEL(label),
                                         binding.column.alignment ==
                                                 table_alignment::end
                                             ? 1
                                         : binding.column.alignment ==
                                                 table_alignment::center
                                             ? .5
                                             : 0);
                    if (image && binding.column.allow_image) {
                        auto *bytes = g_bytes_new(
                            image->pixels(),
                            image->w() * image->h() * sizeof(rgba));
                        auto *texture = gdk_memory_texture_new(
                            image->w(),
                            image->h(),
                            GDK_MEMORY_R8G8B8A8,
                            bytes,
                            image->w() * sizeof(rgba));
                        gtk_image_set_from_paintable(
                            GTK_IMAGE(picture), GDK_PAINTABLE(texture));
                        g_object_unref(texture);
                        g_bytes_unref(bytes);
                    } else
                        gtk_image_clear(GTK_IMAGE(picture));
                }),
                new column_binding{this, column},
                release_binding,
                GConnectFlags(0));
            auto *native_column = gtk_column_view_column_new(
                column.title.c_str(), factory);
            if (get_column_visibility_menu_enabled())
                gtk_column_view_column_set_header_menu(
                    native_column, G_MENU_MODEL(header_menu));
            gtk_column_view_column_set_fixed_width(native_column,
                                                   column.width);
            gtk_column_view_column_set_resizable(
                native_column,
                get_columns_resizable() && column.resizable);
            g_object_set_data(G_OBJECT(native_column),
                              "native-id",
                              GUINT_TO_POINTER(column.id));
            if (column.sortable) {
                auto *sorter = gtk_custom_sorter_new(
                    +[](gconstpointer, gconstpointer, gpointer) -> int {
                        return 0;
                    },
                    nullptr,
                    nullptr);
                gtk_column_view_column_set_sorter(native_column,
                                                  GTK_SORTER(sorter));
                g_object_unref(sorter);
            }
            g_signal_connect(
                native_column,
                "notify::fixed-width",
                G_CALLBACK(
                    +[](GObject *column, GParamSpec *, gpointer data) {
                        auto &owner = *static_cast<table_view *>(data);
                        if (!state(owner).updating)
                            owner.on_native_column_resize(
                                GPOINTER_TO_UINT(g_object_get_data(
                                    column, "native-id")),
                                gtk_column_view_column_get_fixed_width(
                                    GTK_COLUMN_VIEW_COLUMN(column)));
                    }),
                this);
            gtk_column_view_append_column(view(*this), native_column);
            if (const auto sort = get_sort();
                sort && sort->column == column.id)
                gtk_column_view_sort_by_column(
                    view(*this),
                    native_column,
                    sort->direction == sort_direction::ascending
                        ? GTK_SORT_ASCENDING
                        : GTK_SORT_DESCENDING);
            g_object_unref(native_column);
        }
        g_object_unref(header_menu);
        peer.updating = false;
        apply_selection();
    }
    void table_view::apply_selection() {
        auto &peer = state(*this);
        if (peer.portable_drawing) {
            invalidate();
            return;
        }
        peer.updating = true;
        auto *selection = gtk_column_view_get_model(view(*this));
        gtk_selection_model_unselect_all(selection);
        const auto ids = get_selected_rows();
        for (const auto id : ids) {
            const auto position =
                id == _focused_row && _focused_model_row
                    ? _visible_rows->display_index_for_model_row(
                          *_focused_model_row)
                    : display_row_for_id(id);
            if (position)
                gtk_selection_model_select_item(
                    selection, *position, false);
        }
        peer.updating = false;
    }
    void table_view::apply_scroll() {
        if (state(*this).portable_drawing) {
            invalidate();
            return;
        }
        auto *scroll = GTK_SCROLLED_WINDOW(state(*this).box);
        gtk_adjustment_set_value(
            gtk_scrolled_window_get_hadjustment(scroll),
            get_horizontal_scroll_offset());
        if (get_vertical_scroll_row() < get_display_row_count())
            gtk_column_view_scroll_to(view(*this),
                                      get_vertical_scroll_row(),
                                      nullptr,
                                      GTK_LIST_SCROLL_NONE,
                                      nullptr);
    }
    void table_view::show_native() {
        gtk_widget_set_visible(state(*this).widget, true);
    }
    void table_view::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
