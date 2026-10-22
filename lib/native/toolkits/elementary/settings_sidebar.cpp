//
// Uses GraniteSettingsSidebar with native metadata pages. Native owns the
// actual borrowed page-window lifecycle, while GtkStack drives navigation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../wayland/picker_widgets.h"
#include "../../picker_peer.h"
#include <granite-7.h>
#include <algorithm>

namespace
{
    struct sidebar_state
    {
        native::settings_sidebar *owner = nullptr;
        GtkWidget *widget = nullptr;
        GtkWidget *rows = nullptr;
        GtkStack *stack = nullptr;
        bool updating = false;
        std::vector<native::settings_page> items;
        std::vector<GtkWidget *> pages;
        ~sidebar_state() {
            if (stack) g_signal_handlers_disconnect_by_data(stack, owner);
            if (widget) g_object_unref(widget);
            if (stack) g_object_unref(stack);
        }
    };
    // Granite's abstract page adds metadata but requires no virtual methods.
    // A concrete private C GType lets the C++ adapter use that supported API.
    GType page_type() {
        static const GType type = g_type_register_static_simple(GRANITE_TYPE_SETTINGS_PAGE,
            "NativeSettingsPage", sizeof(GraniteSettingsPageClass), nullptr,
            sizeof(GraniteSettingsPage), nullptr, GTypeFlags(0));
        return type;
    }
    bool same_items(const sidebar_state &peer, const native::settings_sidebar &owner) {
        const auto items = native::detail::settings_visible_items(owner);
        if (!peer.widget || peer.items.size() != items.size()) return false;
        for (std::size_t index = 0; index < peer.items.size(); ++index) {
            const auto &a = peer.items[index], &b = items[index];
            if (a.id != b.id || a.title != b.title || a.image != b.image ||
                a.section != b.section || a.enabled != b.enabled) return false;
        }
        return true;
    }
    GtkWidget *picture(const std::shared_ptr<const native::img> &image) {
        auto *widget = gtk_image_new();
        gtk_image_set_pixel_size(GTK_IMAGE(widget), 24);
        gtk_widget_set_halign(widget, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
        if (image && image->w() && image->h()) {
            auto *bytes = g_bytes_new(image->pixels(), image->w() * image->h() * sizeof(native::rgba));
            auto *texture = gdk_memory_texture_new(image->w(), image->h(), GDK_MEMORY_R8G8B8A8,
                bytes, image->w() * sizeof(native::rgba));
            gtk_image_set_from_paintable(GTK_IMAGE(widget), GDK_PAINTABLE(texture));
            g_object_unref(texture); g_bytes_unref(bytes);
        }
        return widget;
    }
    void rebuild(native::settings_sidebar &owner, sidebar_state &peer) {
        if (peer.stack) g_signal_handlers_disconnect_by_data(peer.stack, &owner);
        if (peer.widget) {
            gtk_fixed_remove(GTK_FIXED(linux::wayland::state(owner).content), peer.widget);
            g_object_unref(peer.widget); peer.widget = nullptr;
        }
        if (peer.stack) g_object_unref(peer.stack);
        peer.stack = GTK_STACK(g_object_ref_sink(gtk_stack_new()));
        peer.pages.clear(); peer.items = native::detail::settings_visible_items(owner);
        std::string section;
        for (const auto &item : peer.items) {
            const char *header = item.section != section && !item.section.empty() ? item.section.c_str() : nullptr;
            auto *page = GTK_WIDGET(g_object_new(page_type(), "title", item.title.c_str(),
                "display-widget", picture(item.image), "header", header, nullptr));
            section = item.section;
            peer.pages.push_back(page);
            gtk_stack_add_named(peer.stack, page, std::to_string(item.id).c_str());
        }
        peer.widget = GTK_WIDGET(g_object_ref_sink(granite_settings_sidebar_new(peer.stack)));
        // Granite 7 rows rely on the desktop's sidebar CSS for padding.
        // Keep its colors, selection and scaled insets owned by the theme.
        gtk_widget_add_css_class(peer.widget, "sidebar");
        peer.rows = linux::wayland::find_widget(peer.widget, GTK_TYPE_LIST_BOX);
        gtk_fixed_put(GTK_FIXED(linux::wayland::state(owner).content), peer.widget, 0, 0);
        for (std::size_t index = 0; index < peer.items.size(); ++index) {
            auto *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(peer.rows), index);
            auto *content = gtk_list_box_row_get_child(row);
            // The GTK4 port removed the GTK3 row's six-pixel column gap.
            // Restore that gap when Granite supplies no native spacing.
            if (GTK_IS_GRID(content) && !gtk_grid_get_column_spacing(GTK_GRID(content)))
                gtk_grid_set_column_spacing(GTK_GRID(content), 6);
            gtk_widget_set_sensitive(GTK_WIDGET(row), peer.items[index].enabled);
        }
        g_signal_connect(peer.stack, "notify::visible-child", G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
            auto &owner = *static_cast<native::settings_sidebar *>(data);
            auto &peer = *native::detail::peer_state<sidebar_state>(owner);
            if (peer.updating) return;
            auto *visible = gtk_stack_get_visible_child(peer.stack);
            for (std::size_t index = 0; index < peer.pages.size(); ++index)
                if (peer.pages[index] == visible) { owner.on_native_selection(peer.items[index].id); return; }
        }), &owner);
    }
}
namespace native::detail
{
    bool settings_sidebar_uses_native_search() { return false; }
    int settings_sidebar_navigation_padding() { return 0; }
    bool settings_sidebar_uses_list() { return false; }
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        auto *peer = new sidebar_state; peer->owner = &owner;
        assign_peer_state(owner, peer); return true;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *peer = peer_state<sidebar_state>(owner);
        if (!peer) return;
        peer->updating = true;
        if (!same_items(*peer, owner)) rebuild(owner, *peer);
        const auto area = settings_navigation_bounds(owner);
        gtk_fixed_move(GTK_FIXED(linux::wayland::state(owner).content), peer->widget, area.x1(), area.y1());
        gtk_widget_set_size_request(peer->widget, std::min(owner.get_sidebar_width(), int(area.w())), area.h());
        int selected = -1;
        for (std::size_t index = 0; index < peer->items.size(); ++index)
            if (peer->items[index].id == owner.get_selected_item()) selected = index;
        // Granite assumes row-selected carries a row. Block that handler
        // while clearing selection, which Native permits for an empty page.
        const auto signal = g_signal_lookup("row-selected", GTK_TYPE_LIST_BOX);
        if (selected < 0) g_signal_handlers_block_matched(peer->rows, G_SIGNAL_MATCH_ID, signal, 0, nullptr, nullptr, nullptr);
        gtk_list_box_select_row(GTK_LIST_BOX(peer->rows), selected < 0 ? nullptr
            : gtk_list_box_get_row_at_index(GTK_LIST_BOX(peer->rows), selected));
        if (selected < 0) g_signal_handlers_unblock_matched(peer->rows, G_SIGNAL_MATCH_ID, signal, 0, nullptr, nullptr, nullptr);
        peer->updating = false;
    }
}
