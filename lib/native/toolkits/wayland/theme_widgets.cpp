//
// Builds native widget probes for complete-control and CSS-part
// rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "gtk_theme.h"
#include <cstring>
namespace linux::wayland
{
    GtkWidget *find_theme_widget(GtkWidget *widget, const char *name) {
        if (std::strcmp(gtk_widget_get_css_name(widget), name) == 0)
            return widget;
        for (auto *child = gtk_widget_get_first_child(widget); child;
             child = gtk_widget_get_next_sibling(child))
            if (auto *found = find_theme_widget(child, name))
                return found;
        return nullptr;
    }
    GtkWidget *theme_list(const std::vector<std::string> &items,
                          int selected) {
        auto *list = gtk_list_box_new();
        for (const auto &text : items) {
            auto *label = gtk_label_new(text.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_list_box_append(GTK_LIST_BOX(list), label);
        }
        gtk_list_box_select_row(GTK_LIST_BOX(list),
                                gtk_list_box_get_row_at_index(
                                    GTK_LIST_BOX(list), selected));
        return list;
    }
    GtkWidget *theme_radio(const std::string &text, bool selected) {
        auto *radio = gtk_check_button_new_with_label(text.c_str());
        auto *group = gtk_check_button_new();
        g_object_ref_sink(group);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio),
                                   GTK_CHECK_BUTTON(group));
        g_object_set_data_full(G_OBJECT(radio),
                               "native-radio-group",
                               group,
                               g_object_unref);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(radio), selected);
        return radio;
    }
    GtkWidget *theme_grid(bool selected) {
        const char *items[] = {"", nullptr};
        auto *model = gtk_string_list_new(items);
        auto *selection = gtk_single_selection_new(G_LIST_MODEL(model));
        gtk_single_selection_set_autoselect(selection, false);
        gtk_single_selection_set_can_unselect(selection, true);
        gtk_single_selection_set_selected(
            selection, selected ? 0 : GTK_INVALID_LIST_POSITION);
        auto *factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory,
                         "setup",
                         G_CALLBACK(+[](GtkSignalListItemFactory *,
                                        GtkListItem *item,
                                        gpointer) {
                             gtk_list_item_set_child(item,
                                                     gtk_label_new(""));
                         }),
                         nullptr);
        auto *grid =
            gtk_grid_view_new(GTK_SELECTION_MODEL(selection), factory);
        gtk_grid_view_set_min_columns(GTK_GRID_VIEW(grid), 1);
        gtk_grid_view_set_max_columns(GTK_GRID_VIEW(grid), 1);
        return grid;
    }
    GtkWidget *theme_table() {
        const char *items[] = {"M", nullptr};
        auto *selection = gtk_single_selection_new(
            G_LIST_MODEL(gtk_string_list_new(items)));
        auto *view =
            gtk_column_view_new(GTK_SELECTION_MODEL(selection));
        auto *factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory,
                         "setup",
                         G_CALLBACK(+[](GtkSignalListItemFactory *,
                                        GtkListItem *item,
                                        gpointer) {
                             gtk_list_item_set_child(
                                 item, gtk_label_new("M"));
                         }),
                         nullptr);
        auto *column = gtk_column_view_column_new("M", factory);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(view), column);
        g_object_unref(column);
        return view;
    }
    GtkWidget *
    theme_column_header(native::sort_indicator_state direction) {
        auto *view = gtk_column_view_new(nullptr);
        auto *column = gtk_column_view_column_new("", nullptr);
        auto *sorter = gtk_custom_sorter_new(
            +[](gconstpointer, gconstpointer, gpointer) -> int {
                return 0;
            },
            nullptr,
            nullptr);
        gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
        g_object_unref(sorter);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(view), column);
        gtk_column_view_sort_by_column(
            GTK_COLUMN_VIEW(view),
            column,
            direction == native::sort_indicator_state::ascending
                ? GTK_SORT_ASCENDING
                : GTK_SORT_DESCENDING);
        gtk_column_view_column_set_expand(column, true);
        g_object_unref(column);
        return view;
    }
    void theme_menu_actions(GtkWidget *menu) {
        auto *actions = g_simple_action_group_new();
        auto *action = g_simple_action_new("placeholder", nullptr);
        g_action_map_add_action(G_ACTION_MAP(actions),
                                G_ACTION(action));
        g_object_unref(action);
        gtk_widget_insert_action_group(
            menu, "native", G_ACTION_GROUP(actions));
        g_object_unref(actions);
    }
    std::string theme_menu_label(const std::string &text) {
        std::string result;
        for (char character : text) {
            if (character == '_')
                result.push_back('_');
            result.push_back(character);
        }
        return result;
    }
    GtkWidget *theme_menu(const std::string &title) {
        auto *model = g_menu_new();
        if (!title.empty()) {
            auto *submenu = g_menu_new();
            g_menu_append(submenu, "", "native.placeholder");
            g_menu_append_submenu(model,
                                  theme_menu_label(title).c_str(),
                                  G_MENU_MODEL(submenu));
            g_object_unref(submenu);
        }
        auto *bar =
            gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(model));
        g_object_unref(model);
        theme_menu_actions(bar);
        return bar;
    }
} // namespace linux::wayland
