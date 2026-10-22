//
// Hosts the canonical portable source document in GtkSourceView 5.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include <gtksourceview/gtksource.h>
#include <typeinfo>
namespace linux::wayland
{
    void refresh_source(native::code_edit &owner) {
        auto &peer = state(owner);
        if (peer.synchronize)
            peer.synchronize();
    }
} // namespace linux::wayland
namespace native
{
    void code_edit::create_native() {
        auto *view = gtk_source_view_new();
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), true);
        auto *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      view);
        auto *overlay = gtk_overlay_new();
        auto *drawing = gtk_drawing_area_new();
        gtk_widget_set_hexpand(drawing, true);
        gtk_widget_set_vexpand(drawing, true);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), scroll);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);
        linux::wayland::attach(*this, overlay);
        linux::wayland::state(*this).editor = view;
        linux::wayland::state(*this).drawing = drawing;
        linux::wayland::state(*this).box = scroll;
        linux::wayland::configure_drawing(*this);
        linux::wayland::state(*this).synchronize = [this] {
            auto &peer = linux::wayland::state(*this);
            if (peer.updating || !peer.editor)
                return;
            peer.updating = true;
            // The stock editor uses GtkSourceView. Portable overlays
            // and derived drawing contracts require the shared semantic
            // painter.
            const auto &colors = get_code_theme();
            const bool portable =
                typeid(*this) != typeid(code_edit) || get_lexer() ||
                !markers().empty() || !diagnostics().empty() ||
                !style_runs().empty() || get_completion_visible() ||
                !colors.styles.empty() || colors.gutter_background.a ||
                colors.gutter_text.a || colors.current_line.a ||
                colors.marker.a;
            gtk_widget_set_visible(peer.drawing, portable);
            gtk_widget_set_visible(peer.box, !portable);
            if (get_completion_visible() && !peer.completion_visible)
                gtk_widget_grab_focus(peer.drawing);
            peer.completion_visible = get_completion_visible();

            auto *view = GTK_SOURCE_VIEW(peer.editor);
            auto *buffer =
                gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
            GtkTextIter first;
            GtkTextIter last;
            gtk_text_buffer_get_bounds(buffer, &first, &last);
            char *text =
                gtk_text_buffer_get_text(buffer, &first, &last, true);
            if (this->get_text() != text)
                gtk_text_buffer_set_text(buffer,
                                         this->get_text().c_str(),
                                         this->get_text().size());
            g_free(text);
            gtk_source_view_set_show_line_numbers(
                view, this->get_show_line_numbers());
            gtk_source_view_set_tab_width(view, this->get_tab_width());
            gtk_text_view_set_editable(GTK_TEXT_VIEW(view),
                                       !this->get_read_only());
            auto *manager = gtk_source_language_manager_get_default();
            gtk_source_buffer_set_language(
                GTK_SOURCE_BUFFER(buffer),
                gtk_source_language_manager_get_language(
                    manager, this->get_language().c_str()));
            GtkTextIter caret;
            GtkTextIter anchor;
            const auto &value = get_text();
            const auto caret_chars = g_utf8_pointer_to_offset(
                value.c_str(),
                value.c_str() + std::min(_caret, value.size()));
            const auto anchor_chars = g_utf8_pointer_to_offset(
                value.c_str(),
                value.c_str() + std::min(_anchor, value.size()));
            gtk_text_buffer_get_iter_at_offset(
                buffer, &caret, caret_chars);
            gtk_text_buffer_get_iter_at_offset(
                buffer, &anchor, anchor_chars);
            gtk_text_buffer_select_range(buffer, &caret, &anchor);
            peer.updating = false;
        };
        linux::wayland::refresh_source(*this);
        auto *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        g_signal_connect(
            buffer,
            "changed",
            G_CALLBACK(+[](GtkTextBuffer *buffer, gpointer data) {
                auto &owner = *static_cast<code_edit *>(data);
                auto &peer = linux::wayland::state(owner);
                if (peer.updating)
                    return;
                GtkTextIter first;
                GtkTextIter last;
                gtk_text_buffer_get_bounds(buffer, &first, &last);
                char *text = gtk_text_buffer_get_text(
                    buffer, &first, &last, true);
                std::string value(text);
                g_free(text);
                const detail::window_guard guard(owner);
                peer.updating = true;
                const bool accepted = owner.on_native_text(value);
                if (!guard.alive())
                    return;
                linux::wayland::state(owner).updating = false;
                if (!accepted)
                    linux::wayland::refresh_source(owner);
            }),
            this);
        g_signal_connect(
            buffer,
            "mark-set",
            G_CALLBACK(+[](GtkTextBuffer *buffer,
                           GtkTextIter *,
                           GtkTextMark *,
                           gpointer data) {
                auto &owner = *static_cast<code_edit *>(data);
                if (linux::wayland::state(owner).updating)
                    return;
                GtkTextIter start;
                GtkTextIter caret;
                GtkTextIter anchor;
                gtk_text_buffer_get_start_iter(buffer, &start);
                gtk_text_buffer_get_iter_at_mark(
                    buffer, &caret, gtk_text_buffer_get_insert(buffer));
                gtk_text_buffer_get_iter_at_mark(
                    buffer,
                    &anchor,
                    gtk_text_buffer_get_selection_bound(buffer));
                char *before = gtk_text_buffer_get_text(
                    buffer, &start, &caret, true);
                owner._caret = std::char_traits<char>::length(before);
                g_free(before);
                before = gtk_text_buffer_get_text(
                    buffer, &start, &anchor, true);
                owner._anchor = std::char_traits<char>::length(before);
                g_free(before);
            }),
            this);
    }
    void code_edit::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void code_edit::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
