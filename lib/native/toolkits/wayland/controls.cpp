//
// Uses native GTK buttons, checks, radios, lists and combo controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include <algorithm>
namespace
{
    using namespace linux::wayland;
    GtkWidget *widget(native::wnd &owner) {
        return state(owner).widget;
    }
    void shown(native::wnd &owner) {
        gtk_widget_set_visible(widget(owner), true);
    }
    void combo_changed(GObject *object, GParamSpec *, gpointer data) {
        auto &owner = *static_cast<native::combo_box *>(data);
        if (!state(owner).updating)
            owner.on_native_selection(static_cast<int>(
                gtk_drop_down_get_selected(GTK_DROP_DOWN(object))));
    }
} // namespace
namespace native
{
    void button::create_native() {
        auto *native_button = gtk_button_new_with_label(_text.c_str());
        attach(*this, native_button);
        state(*this).editor = native_button;
        state(*this).layout_children = [this] {
            linux::wayland::style_button_frame(*this, get_border_sides());
        };
        state(*this).layout_children();
        g_signal_connect(
            native_button,
            "clicked",
            G_CALLBACK(+[](GtkButton *, gpointer data) {
                static_cast<button *>(data)->on_native_click();
            }),
            this);
    }
    void button::apply_text() {
        gtk_button_set_label(GTK_BUTTON(state(*this).editor), _text.c_str());
    }
    void button::show_native() {
        gtk_widget_set_visible(state(*this).editor, true);
        shown(*this);
    }
    void button::destroy_native() {
        linux::wayland::destroy(*this);
    }

    void check::create_native() {
        attach(*this, gtk_check_button_new_with_label(_text.c_str()));
        apply_checked();
        g_signal_connect(
            widget(*this),
            "toggled",
            G_CALLBACK(+[](GtkCheckButton *w, gpointer data) {
                auto &owner = *static_cast<check *>(data);
                if (!state(owner).updating)
                    owner.on_native_checked(
                        gtk_check_button_get_active(w));
            }),
            this);
    }
    void check::apply_text() {
        gtk_check_button_set_label(GTK_CHECK_BUTTON(widget(*this)),
                                   _text.c_str());
    }
    void check::apply_checked() {
        auto &peer = state(*this);
        peer.updating = true;
        gtk_check_button_set_active(GTK_CHECK_BUTTON(peer.widget),
                                    _checked);
        peer.updating = false;
    }
    void check::show_native() {
        shown(*this);
    }
    void check::destroy_native() {
        linux::wayland::destroy(*this);
    }

    void radio::create_native() {
        auto *w = gtk_check_button_new_with_label(_text.c_str());
        attach(*this, w);
        // Grouping is owned by the portable radio model; GTK supplies
        // the native radio appearance without imposing a second group
        // model.
        auto *group = gtk_check_button_new();
        g_object_ref_sink(group);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(w),
                                   GTK_CHECK_BUTTON(group));
        g_object_set_data_full(
            G_OBJECT(w), "native-radio-group", group, g_object_unref);
        apply_selected();
        g_signal_connect(
            w,
            "toggled",
            G_CALLBACK(+[](GtkCheckButton *w, gpointer data) {
                auto &owner = *static_cast<radio *>(data);
                if (!state(owner).updating &&
                    gtk_check_button_get_active(w))
                    owner.on_native_selected();
            }),
            this);
    }
    void radio::apply_text() {
        gtk_check_button_set_label(GTK_CHECK_BUTTON(widget(*this)),
                                   _text.c_str());
    }
    void radio::apply_selected() {
        auto &peer = state(*this);
        peer.updating = true;
        gtk_check_button_set_active(GTK_CHECK_BUTTON(peer.widget),
                                    _selected);
        peer.updating = false;
    }
    void radio::show_native() {
        shown(*this);
    }
    void radio::destroy_native() {
        linux::wayland::destroy(*this);
    }

    void list::create_native() {
        auto *scroll = gtk_scrolled_window_new();
        auto *rows = gtk_list_box_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      rows);
        attach(*this, scroll);
        state(*this).editor = rows;
        apply_items();
        apply_selected_index();
        g_signal_connect(
            rows,
            "row-selected",
            G_CALLBACK(
                +[](GtkListBox *, GtkListBoxRow *row, gpointer data) {
                    auto &owner = *static_cast<list *>(data);
                    if (!state(owner).updating)
                        owner.on_native_selection(
                            row ? gtk_list_box_row_get_index(row) : -1);
                }),
            this);
    }
    void list::apply_items() {
        auto &peer = state(*this);
        peer.updating = true;
        while (auto *child = gtk_widget_get_first_child(peer.editor))
            gtk_list_box_remove(GTK_LIST_BOX(peer.editor), child);
        for (const auto &label : get_items()) {
            auto *text = gtk_label_new(label.c_str());
            gtk_label_set_xalign(GTK_LABEL(text), 0);
            gtk_list_box_append(GTK_LIST_BOX(peer.editor), text);
        }
        peer.updating = false;
        apply_selected_index();
    }
    void list::apply_selected_index() {
        auto &peer = state(*this);
        peer.updating = true;
        gtk_list_box_select_row(
            GTK_LIST_BOX(peer.editor),
            gtk_list_box_get_row_at_index(GTK_LIST_BOX(peer.editor),
                                          get_selected_index()));
        peer.updating = false;
    }
    void list::show_native() {
        shown(*this);
    }
    void list::destroy_native() {
        linux::wayland::destroy(*this);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    void combo_box::create_native() {
        auto *host = gtk_fixed_new();
        attach(*this, host);
        state(*this).layout_children = [this] {
            auto &peer = state(*this);
            if (!peer.editor) return;
            gtk_widget_set_size_request(peer.editor, get_dimensions().w, get_dimensions().h);
            int height = 0;
            gtk_widget_measure(peer.editor, GTK_ORIENTATION_VERTICAL,
                get_dimensions().w, nullptr, &height, nullptr, nullptr);
            gtk_fixed_move(GTK_FIXED(peer.widget), peer.editor, 0,
                std::max(0, (int(get_dimensions().h) - height) / 2));
        };
        apply_style();
    }
    void combo_box::apply_items() {
        auto &peer = state(*this);
        peer.updating = true;
        if (GTK_IS_COMBO_BOX_TEXT(peer.editor)) {
            gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(peer.editor));
            for (const auto &item : get_items())
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(peer.editor), item.c_str());
        } else {
            auto *model = gtk_string_list_new(nullptr);
            for (const auto &item : get_items()) gtk_string_list_append(model, item.c_str());
            gtk_drop_down_set_model(GTK_DROP_DOWN(peer.editor), G_LIST_MODEL(model));
            g_object_unref(model);
        }
        peer.updating = false;
        apply_selected_index();
    }
    void combo_box::apply_selected_index() {
        auto &peer = state(*this);
        peer.updating = true;
        if (GTK_IS_COMBO_BOX(peer.editor)) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(peer.editor), get_selected_index());
            gtk_editable_set_text(GTK_EDITABLE(peer.drawing), get_text().c_str());
        } else gtk_drop_down_set_selected(GTK_DROP_DOWN(peer.editor),
            get_selected_index() < 0 ? GTK_INVALID_LIST_POSITION : guint(get_selected_index()));
        peer.updating = false;
    }
    void combo_box::apply_text() {
        auto &peer = state(*this);
        if (!peer.drawing) return;
        peer.updating = true;
        gtk_editable_set_text(GTK_EDITABLE(peer.drawing), get_text().c_str());
        peer.updating = false;
    }
    void combo_box::apply_style() {
        auto &peer = state(*this);
        if (peer.editor) gtk_fixed_remove(GTK_FIXED(peer.widget), peer.editor);
        peer.drawing = nullptr;
        if (get_style() == combo_box_style::editable) {
            // GTK 4.14 retains this native editable combo. GtkDropDown
            // does not provide an editable field with a shared popup.
            peer.editor = gtk_combo_box_text_new_with_entry();
            peer.drawing = gtk_combo_box_get_child(GTK_COMBO_BOX(peer.editor));
            g_signal_connect(peer.editor, "changed", G_CALLBACK(+[](GtkComboBox *widget, gpointer data) {
                auto &owner = *static_cast<combo_box *>(data);
                if (!state(owner).updating && gtk_combo_box_get_active(widget) >= 0)
                    owner.on_native_selection(gtk_combo_box_get_active(widget));
            }), this);
            g_signal_connect(peer.drawing, "changed", G_CALLBACK(+[](GtkEditable *entry, gpointer data) {
                auto &owner = *static_cast<combo_box *>(data);
                auto &peer = state(owner);
                if (peer.updating) return;
                const detail::window_guard guard(owner);
                owner.on_native_text(gtk_editable_get_text(entry));
                if (!guard.alive()) return;
                auto &current = state(owner);
                if (gtk_combo_box_get_active(GTK_COMBO_BOX(current.editor)) == owner.get_selected_index())
                    return;
                const int cursor = gtk_editable_get_position(entry);
                int start = 0;
                int end = 0;
                const bool selected = gtk_editable_get_selection_bounds(entry, &start, &end);
                current.updating = true;
                gtk_combo_box_set_active(GTK_COMBO_BOX(current.editor), owner.get_selected_index());
                if (owner.get_text() != gtk_editable_get_text(GTK_EDITABLE(current.drawing)))
                    gtk_editable_set_text(GTK_EDITABLE(current.drawing), owner.get_text().c_str());
                if (selected) gtk_editable_select_region(entry, start, end);
                else gtk_editable_set_position(entry, cursor);
                current.updating = false;
            }), this);
        } else {
            peer.editor = gtk_drop_down_new(nullptr, nullptr);
            g_signal_connect(peer.editor, "notify::selected", G_CALLBACK(combo_changed), this);
        }
        gtk_fixed_put(GTK_FIXED(peer.widget), peer.editor, 0, 0);
        apply_items(); apply_text();
        peer.layout_children();
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
    void combo_box::show_native() {
        shown(*this);
    }
    void combo_box::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
