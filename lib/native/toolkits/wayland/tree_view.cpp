//
// Adapts the stable-id tree model to native GTK rows and disclosure
// buttons and image-backed native GtkImage children.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include <algorithm>
namespace
{
    using namespace linux::wayland;
    struct tree_action
    {
        native::tree_view *owner;
        native::tree_item_id id;
    };
    void free_action(gpointer data, GClosure *) {
        delete static_cast<tree_action *>(data);
    }
    // Copy portable pixels into an owned native texture. GtkImage scales
    // the paintable with the tree's logical icon size, not its source size.
    GtkWidget *item_image(const native::img &source,
                          native::size dimensions) {
        auto *bytes = g_bytes_new(
            source.pixels(), source.w() * source.h() * sizeof(native::rgba));
        auto *texture = gdk_memory_texture_new(source.w(), source.h(),
            GDK_MEMORY_R8G8B8A8, bytes, source.w() * sizeof(native::rgba));
        auto *image = gtk_image_new_from_paintable(GDK_PAINTABLE(texture));
        gtk_image_set_pixel_size(GTK_IMAGE(image),
                                std::max(dimensions.w, dimensions.h));
        gtk_widget_set_size_request(image, dimensions.w, dimensions.h);
        gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
        g_object_unref(texture);
        g_bytes_unref(bytes);
        return image;
    }
} // namespace
namespace native
{
    void tree_view::create_native() {
        auto *scroll = gtk_scrolled_window_new();
        auto *rows = gtk_list_box_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      rows);
        attach(*this, scroll);
        state(*this).editor = rows;
        synchronize_theme_metrics();
        apply_items();
        g_signal_connect(
            rows,
            "row-selected",
            G_CALLBACK(
                +[](GtkListBox *, GtkListBoxRow *row, gpointer data) {
                    auto &owner = *static_cast<tree_view *>(data);
                    if (state(owner).updating || !row)
                        return;
                    auto *id = static_cast<tree_item_id *>(
                        g_object_get_data(G_OBJECT(row), "native-id"));
                    if (id)
                        owner.on_native_selection(*id);
                }),
            this);
        g_signal_connect(
            rows,
            "row-activated",
            G_CALLBACK(+[](GtkListBox *,
                           GtkListBoxRow *row,
                           gpointer data) {
                auto *id = static_cast<tree_item_id *>(
                    g_object_get_data(G_OBJECT(row), "native-id"));
                if (id)
                    static_cast<tree_view *>(data)->on_native_activate(
                        *id);
            }),
            this);
    }
    void tree_view::apply_items() {
        auto &peer = state(*this);
        peer.updating = true;
        while (auto *child = gtk_widget_get_first_child(peer.editor))
            gtk_list_box_remove(GTK_LIST_BOX(peer.editor), child);
        for (std::size_t i = 0; i < get_visible_item_count(); ++i) {
            const auto visible = get_visible_item(i);
            const auto &item = get_item(visible.id);
            auto *row = gtk_list_box_row_new();
            auto *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_widget_set_margin_start(box, visible.depth * 20);
            if (!item.children.empty()) {
                auto *button = gtk_button_new_from_icon_name(
                    item.expanded ? "pan-down-symbolic"
                                  : "pan-end-symbolic");
                gtk_widget_add_css_class(button, "flat");
                gtk_box_append(GTK_BOX(box), button);
                g_signal_connect_data(
                    button,
                    "clicked",
                    G_CALLBACK(+[](GtkButton *, gpointer data) {
                        auto action = *static_cast<tree_action *>(data);
                        action.owner->on_native_expansion(
                            action.id,
                            !action.owner->get_expanded(action.id));
                    }),
                    new tree_action{this, item.id},
                    free_action,
                    GConnectFlags(0));
            }
            if (item.image && item.image->w() && item.image->h())
                gtk_box_append(GTK_BOX(box),
                               item_image(*item.image, get_icon_size()));
            auto *label = gtk_label_new(item.text.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_widget_set_hexpand(label, true);
            gtk_box_append(GTK_BOX(box), label);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            gtk_widget_set_sensitive(row, item.enabled);
            g_object_set_data_full(
                G_OBJECT(row),
                "native-id",
                new tree_item_id(item.id),
                +[](gpointer value) {
                    delete static_cast<tree_item_id *>(value);
                });
            gtk_list_box_append(GTK_LIST_BOX(peer.editor), row);
        }
        peer.updating = false;
        apply_selection();
    }
    void tree_view::apply_selection() {
        auto &peer = state(*this);
        peer.updating = true;
        for (auto *row = gtk_widget_get_first_child(peer.editor); row;
             row = gtk_widget_get_next_sibling(row)) {
            auto *id = static_cast<tree_item_id *>(
                g_object_get_data(G_OBJECT(row), "native-id"));
            if (id && *id == get_selected_item())
                gtk_list_box_select_row(GTK_LIST_BOX(peer.editor),
                                        GTK_LIST_BOX_ROW(row));
        }
        peer.updating = false;
    }
    void tree_view::apply_expansion(tree_item_id, bool) {
        apply_items();
    }
    void tree_view::apply_scroll_offset() {
        auto *adjustment = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(state(*this).widget));
        gtk_adjustment_set_value(adjustment, get_scroll_offset());
    }
    void tree_view::show_native() {
        gtk_widget_set_visible(state(*this).widget, true);
    }
    void tree_view::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
