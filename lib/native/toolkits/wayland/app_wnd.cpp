//
// Creates Wayland top levels with GTK header bars and owned windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
namespace native
{
    void app_wnd::apply_title() {
        gtk_window_set_title(
            GTK_WINDOW(linux::wayland::state(*this).widget),
            _title.c_str());
    }
    void app_wnd::create_native() {
        validate_owner_created();
        linux::wayland::initialize();
        auto *window =
            gtk_application_window_new(linux::wayland::application);
        auto *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        auto *content = gtk_fixed_new();
        gtk_widget_set_hexpand(content, true);
        gtk_widget_set_vexpand(content, true);
        auto *overlay = gtk_overlay_new();
        auto *drawing = gtk_drawing_area_new();
        gtk_widget_set_hexpand(overlay, true);
        gtk_widget_set_vexpand(overlay, true);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), content);
        gtk_overlay_set_measure_overlay(
            GTK_OVERLAY(overlay), content, false);
        gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), content, true);
        gtk_box_append(GTK_BOX(box), overlay);
        gtk_window_set_child(GTK_WINDOW(window), box);
        gtk_window_set_title(GTK_WINDOW(window), _title.c_str());
        if (get_native_title_visible())
            gtk_window_set_titlebar(GTK_WINDOW(window),
                                    gtk_header_bar_new());
        else
            gtk_window_set_decorated(GTK_WINDOW(window), false);
        if (auto *owner = get_owner(); owner && get_modal()) {
            gtk_window_set_transient_for(
                GTK_WINDOW(window),
                GTK_WINDOW(linux::wayland::state(*owner).widget));
            gtk_window_set_modal(GTK_WINDOW(window), get_modal());
        }
        auto *peer = new linux::wayland::widget_state;
        peer->widget = GTK_WIDGET(g_object_ref(window));
        peer->content = content;
        peer->box = box;
        peer->drawing = drawing;
        detail::assign_peer_state(*this, peer);
        detail::wnd_peer_access::assign_content(*this, content);
        linux::wayland::wnd_bindings.register_pair(window, this);
        linux::wayland::install_keyboard(*this, window);
        linux::wayland::configure_drawing(*this);
        linux::wayland::geometry(*this);
        linux::wayland::configure_desktop(GTK_WINDOW(window));
        g_signal_connect(
            window,
            "close-request",
            G_CALLBACK(+[](GtkWindow *, gpointer data) -> gboolean {
                static_cast<app_wnd *>(data)->request_close();
                return true;
            }),
            this);
        menu.attach(*this);
    }
    void app_wnd::show_native() {
        linux::wayland::geometry(*this);
        gtk_window_present(
            GTK_WINDOW(linux::wayland::state(*this).widget));
    }
    void app_wnd::destroy_native() {
        destroy_owned_windows();
        destroy_children();
        linux::wayland::destroy(*this);
        if (this == app::main_wnd()) {
            linux::wayland::exit_requested = true;
            g_main_context_wakeup(nullptr);
        }
    }
} // namespace native
