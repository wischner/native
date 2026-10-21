//
// Private GTK4 Wayland peers; no toolkit types cross the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>
#include <gtk/gtk.h>
#include <gdk/wayland/gdkwayland.h>
#include <bindings.h>
#include "../../wnd_peer.h"

namespace linux::wayland
{
    class theme_snapshot;
    struct widget_state
    {
        // Release the peer's GTK reference if creation unwinds early.
        ~widget_state();

        // Own one reference independently from the GTK parent hierarchy.
        GtkWidget *widget = nullptr;
        GtkWidget *content = nullptr;
        GtkWidget *drawing = nullptr;
        GtkWidget *box = nullptr;
        GtkWidget *menu = nullptr;
        GtkWidget *editor = nullptr;
        GtkEventController *shortcuts = nullptr;
        cairo_t *context = nullptr;
        std::shared_ptr<theme_snapshot> theme_cache;
        bool updating = false;
        bool portable_drawing = false;
        bool dragging_scrollbar = false;
        bool horizontal_scrollbar = false;
        int scrollbar_grab = 0;
        std::function<void()> synchronize;
        std::vector<GtkWidget *> pages;
    };
    extern native::bindings<GtkWidget *, native::wnd *> wnd_bindings;
    extern bool exit_requested;
    extern GtkApplication *application;
    // Initialize GTK with the Wayland backend exclusively.
    void initialize();
    // Present the desktop-native owner-modal message control.
    native::message_box_result
    show_message(native::app_wnd &owner,
                 const std::string &message,
                 const std::string &title,
                 native::message_box_buttons buttons,
                 native::message_box_icon icon);
    // Create the desktop-native single-line editor.
    GtkWidget *new_entry();
    // Apply the selected desktop's integration to a new window.
    void configure_desktop(GtkWindow *window);
    // Return an existing peer, or throw for a missing resource.
    widget_state &state(const native::wnd &owner);
    // Attach a GTK widget to its portable parent and assign owned peer
    // state.
    void attach(native::wnd &owner,
                GtkWidget *widget,
                GtkWidget *content = nullptr);
    // Configure painting for a peer whose drawing widget is assigned.
    void configure_drawing(native::wnd &owner);
    // Create a drawing region with an overlaid fixed child host.
    void create_surface(native::wnd &owner);
    // Move a live widget through its native container APIs.
    void reparent(native::wnd &owner);
    // Release a child widget and unregister callback lookup.
    void destroy(native::wnd &owner);
    // Synchronize the canonical document and source-view settings.
    void refresh_source(native::code_edit &owner);
    // Apply cached logical geometry; the compositor places top levels.
    void geometry(native::wnd &owner);
    // Apply portable table navigation and type search from GTK keys.
    bool table_key(native::table_view &owner,
                   guint key,
                   GdkModifierType modifiers);
    // Translate physical input while preserving native GTK text
    // handling.
    void install_keyboard(native::wnd &owner, GtkWidget *widget);
    // Add pointer, keyboard and focus controllers to a drawing region.
    void install_input(native::wnd &owner, GtkWidget *widget);
    // Render stock text using Pango at a top-left position.
    void text(cairo_t *cr,
              const std::string &value,
              native::point p,
              const native::font_t *font,
              native::rgba color);
    // Copy straight-alpha RGBA pixels to Cairo's premultiplied format.
    void image(cairo_t *cr, const native::img &source, native::point p);
} // namespace linux::wayland
