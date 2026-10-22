//
// Adapts the portable icon model to GTK's virtualized native grid view.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "index_model.h"
namespace native
{
    void icon_view::create_native() {
        auto *factory = gtk_signal_list_item_factory_new();
        g_signal_connect(
            factory,
            "setup",
            G_CALLBACK(+[](GtkSignalListItemFactory *,
                           GtkListItem *item,
                           gpointer) {
                auto *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_box_append(GTK_BOX(box), gtk_picture_new());
                gtk_box_append(GTK_BOX(box), gtk_label_new(nullptr));
                gtk_list_item_set_child(item, box);
            }),
            nullptr);
        g_signal_connect(
            factory,
            "bind",
            G_CALLBACK(+[](GtkSignalListItemFactory *,
                           GtkListItem *item,
                           gpointer data) {
                auto &owner = *static_cast<icon_view *>(data);
                const auto index = linux::wayland::item_index(
                    G_OBJECT(gtk_list_item_get_item(item)));
                const auto &value = owner.get_items().at(index);
                auto *box = gtk_list_item_get_child(item);
                auto *picture = gtk_widget_get_first_child(box);
                auto *label = gtk_widget_get_next_sibling(picture);
                gtk_orientable_set_orientation(
                    GTK_ORIENTABLE(box),
                    owner.get_label_mode() ==
                            icon_view_label_mode::beside
                        ? GTK_ORIENTATION_HORIZONTAL
                        : GTK_ORIENTATION_VERTICAL);
                gtk_label_set_text(GTK_LABEL(label),
                                   value.text.c_str());
                gtk_widget_set_visible(
                    label,
                    owner.get_label_mode() !=
                        icon_view_label_mode::hidden);
                gtk_widget_set_sensitive(box, value.enabled);
                gtk_widget_set_size_request(picture,
                                            owner.get_icon_size().w,
                                            owner.get_icon_size().h);
                gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(picture, GTK_ALIGN_CENTER);
                if (value.image) {
                    auto *bytes = g_bytes_new(value.image->pixels(),
                                              value.image->w() *
                                                  value.image->h() *
                                                  sizeof(rgba));
                    auto *texture = gdk_memory_texture_new(
                        value.image->w(),
                        value.image->h(),
                        GDK_MEMORY_R8G8B8A8,
                        bytes,
                        value.image->w() * sizeof(rgba));
                    gtk_picture_set_paintable(GTK_PICTURE(picture),
                                              GDK_PAINTABLE(texture));
                    g_object_unref(texture);
                    g_bytes_unref(bytes);
                } else
                    gtk_picture_set_paintable(GTK_PICTURE(picture),
                                              nullptr);
            }),
            this);
        auto *selection = gtk_single_selection_new(
            linux::wayland::index_model(get_items().size()));
        gtk_single_selection_set_autoselect(selection, false);
        gtk_single_selection_set_can_unselect(selection, true);
        auto *view =
            gtk_grid_view_new(GTK_SELECTION_MODEL(selection), factory);
        auto *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      view);
        linux::wayland::attach(*this, scroll);
        linux::wayland::state(*this).editor = view;
        g_signal_connect(
            selection,
            "notify::selected",
            G_CALLBACK(
                +[](GObject *selection, GParamSpec *, gpointer data) {
                    auto &owner = *static_cast<icon_view *>(data);
                    if (!linux::wayland::state(owner).updating)
                        owner.on_native_selection(static_cast<int>(
                            gtk_single_selection_get_selected(
                                GTK_SINGLE_SELECTION(selection))));
                }),
            this);
        g_signal_connect(
            view,
            "activate",
            G_CALLBACK(
                +[](GtkGridView *, guint position, gpointer data) {
                    static_cast<icon_view *>(data)->on_native_activate(
                        position);
                }),
            this);
        apply_selected_index();
    }
    void icon_view::apply_items() {
        auto &peer = linux::wayland::state(*this);
        peer.updating = true;
        auto *selection = GTK_SINGLE_SELECTION(
            gtk_grid_view_get_model(GTK_GRID_VIEW(peer.editor)));
        auto *model = linux::wayland::index_model(get_items().size());
        gtk_single_selection_set_model(selection, model);
        g_object_unref(model);
        peer.updating = false;
        apply_selected_index();
    }
    void icon_view::apply_icon_size() {
        apply_items();
    }
    void icon_view::apply_label_mode() {
        apply_items();
    }
    void icon_view::apply_selected_index() {
        auto &peer = linux::wayland::state(*this);
        peer.updating = true;
        // Selecting an already-visible tile must not reset the scroll
        // adjustment while an expander is hosting the grid.
        const double offset = gtk_adjustment_get_value(
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(peer.widget)));
        gtk_single_selection_set_selected(
            GTK_SINGLE_SELECTION(
                gtk_grid_view_get_model(GTK_GRID_VIEW(peer.editor))),
            get_selected_index() < 0
                ? GTK_INVALID_LIST_POSITION
                : static_cast<guint>(get_selected_index()));
        peer.updating = false;
        gtk_adjustment_set_value(
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(peer.widget)), offset);
    }
    void icon_view::apply_scroll_offset() {
        gtk_adjustment_set_value(
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(
                linux::wayland::state(*this).widget)),
            get_scroll_offset());
    }
    void icon_view::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void icon_view::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
