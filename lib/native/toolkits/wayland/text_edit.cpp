//
// Adapts GTK entries and text views with live validation and selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../property_grid_peer.h"
#include "globals.h"
namespace
{
    using namespace linux::wayland;
    GtkTextBuffer *buffer(native::text_edit &owner) {
        return gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(state(owner).editor));
    }
    std::string value(native::text_edit &owner) {
        auto *editor = state(owner).editor;
        if (GTK_IS_EDITABLE(editor))
            return gtk_editable_get_text(GTK_EDITABLE(editor));
        GtkTextIter first, last;
        auto *b = buffer(owner);
        gtk_text_buffer_get_bounds(b, &first, &last);
        auto *text = gtk_text_buffer_get_text(b, &first, &last, true);
        std::string result(text);
        g_free(text);
        return result;
    }
} // namespace
namespace native
{
    void text_edit::create_native() {
        GtkWidget *editor = nullptr;
        GtkWidget *host = nullptr;
        if (get_mode() == text_edit_mode::single_line) {
            host = editor = dynamic_cast<settings_sidebar *>(get_parent())
                ? gtk_search_entry_new() : linux::wayland::new_entry();
            if (GTK_IS_SEARCH_ENTRY(editor))
                gtk_widget_set_tooltip_text(editor, "Search settings");
        }
        else {
            editor = gtk_text_view_new();
            host = gtk_scrolled_window_new();
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(host),
                                          editor);
        }
        attach(*this, host);
        state(*this).editor = editor;
        if (native::detail::property_grid_owner(*this)) {
            gtk_widget_add_css_class(host, "flat");
            if (GTK_IS_ENTRY(editor)) gtk_entry_set_has_frame(GTK_ENTRY(editor), false);
        }
        apply_text();
        apply_read_only();
        auto changed = +[](GObject *, gpointer data) {
            auto &owner = *static_cast<text_edit *>(data);
            if (state(owner).updating)
                return;
            const detail::window_guard guard(owner);
            if (!owner.on_native_text(value(owner)) && guard.alive())
                owner.apply_text();
        };
        g_signal_connect(GTK_IS_EDITABLE(editor)
                             ? G_OBJECT(editor)
                             : G_OBJECT(buffer(*this)),
                         "changed",
                         G_CALLBACK(changed),
                         this);
    }
    void text_edit::apply_text() {
        auto &peer = state(*this);
        peer.updating = true;
        if (GTK_IS_EDITABLE(peer.editor))
            gtk_editable_set_text(GTK_EDITABLE(peer.editor),
                                  _text.c_str());
        else
            gtk_text_buffer_set_text(
                buffer(*this), _text.c_str(), _text.size());
        peer.updating = false;
    }
    void text_edit::apply_read_only() {
        auto *w = state(*this).editor;
        if (GTK_IS_EDITABLE(w))
            gtk_editable_set_editable(GTK_EDITABLE(w), !_read_only);
        else
            gtk_text_view_set_editable(GTK_TEXT_VIEW(w), !_read_only);
    }
    void text_edit::show_native() {
        gtk_widget_set_visible(state(*this).widget, true);
    }
    void text_edit::destroy_native() {
        linux::wayland::destroy(*this);
    }
    std::string text_edit::selected_text() const {
        auto &owner = *const_cast<text_edit *>(this);
        auto *w = state(owner).editor;
        if (GTK_IS_EDITABLE(w)) {
            int first, last;
            if (!gtk_editable_get_selection_bounds(
                    GTK_EDITABLE(w), &first, &last))
                return {};
            char *text =
                gtk_editable_get_chars(GTK_EDITABLE(w), first, last);
            std::string result(text);
            g_free(text);
            return result;
        }
        GtkTextIter first, last;
        if (!gtk_text_buffer_get_selection_bounds(
                buffer(owner), &first, &last))
            return {};
        char *text = gtk_text_buffer_get_text(
            buffer(owner), &first, &last, true);
        std::string result(text);
        g_free(text);
        return result;
    }
    bool text_edit::replace_selected_text(const std::string &text) {
        if (_read_only)
            return false;
        auto *w = state(*this).editor;
        int first = 0, last = 0;
        GtkTextIter begin, end;
        if (GTK_IS_EDITABLE(w)) {
            if (!gtk_editable_get_selection_bounds(
                    GTK_EDITABLE(w), &first, &last))
                first = last =
                    gtk_editable_get_position(GTK_EDITABLE(w));
        } else {
            auto *b = buffer(*this);
            if (!gtk_text_buffer_get_selection_bounds(
                    b, &begin, &end)) {
                gtk_text_buffer_get_iter_at_mark(
                    b, &begin, gtk_text_buffer_get_insert(b));
                end = begin;
            }
            first = gtk_text_iter_get_offset(&begin);
            last = gtk_text_iter_get_offset(&end);
        }
        const char *start =
            g_utf8_offset_to_pointer(_text.c_str(), first);
        const char *finish =
            g_utf8_offset_to_pointer(_text.c_str(), last);
        const auto candidate =
            std::string(_text.c_str(), start) + text + finish;
        if (!validate(candidate))
            return false;
        auto &peer = state(*this);
        peer.updating = true;
        if (GTK_IS_EDITABLE(w)) {
            gtk_editable_delete_text(GTK_EDITABLE(w), first, last);
            gtk_editable_insert_text(
                GTK_EDITABLE(w), text.c_str(), text.size(), &first);
            gtk_editable_set_position(GTK_EDITABLE(w), first);
        } else {
            gtk_text_buffer_delete(buffer(*this), &begin, &end);
            gtk_text_buffer_insert(
                buffer(*this), &begin, text.c_str(), text.size());
        }
        peer.updating = false;
        return on_native_text(candidate);
    }
    void text_edit::select_all_native() {
        auto *w = state(*this).editor;
        if (GTK_IS_EDITABLE(w))
            gtk_editable_select_region(GTK_EDITABLE(w), 0, -1);
        else {
            GtkTextIter first, last;
            gtk_text_buffer_get_bounds(buffer(*this), &first, &last);
            gtk_text_buffer_select_range(buffer(*this), &first, &last);
        }
    }
} // namespace native
