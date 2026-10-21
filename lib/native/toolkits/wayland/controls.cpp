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
        attach(*this, gtk_button_new_with_label(_text.c_str()));
        g_signal_connect(
            widget(*this),
            "clicked",
            G_CALLBACK(+[](GtkButton *, gpointer data) {
                static_cast<button *>(data)->on_native_click();
            }),
            this);
    }
    void button::apply_text() {
        gtk_button_set_label(GTK_BUTTON(widget(*this)), _text.c_str());
    }
    void button::show_native() {
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

    void combo_box::create_native() {
        auto *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        auto *entry = linux::wayland::new_entry();
        auto *drop = gtk_drop_down_new(nullptr, nullptr);
        gtk_widget_set_hexpand(entry, true);
        gtk_box_append(GTK_BOX(box), entry);
        gtk_box_append(GTK_BOX(box), drop);
        attach(*this, box);
        state(*this).editor = entry;
        state(*this).drawing = drop;
        apply_items();
        apply_style();
        apply_text();
        apply_selected_index();
        g_signal_connect(
            drop, "notify::selected", G_CALLBACK(combo_changed), this);
        g_signal_connect(
            entry,
            "changed",
            G_CALLBACK(+[](GtkEditable *w, gpointer data) {
                auto &owner = *static_cast<combo_box *>(data);
                if (!state(owner).updating)
                    owner.on_native_text(gtk_editable_get_text(w));
            }),
            this);
    }
    void combo_box::apply_items() {
        auto &peer = state(*this);
        peer.updating = true;
        auto *model = gtk_string_list_new(nullptr);
        for (const auto &item : get_items())
            gtk_string_list_append(model, item.c_str());
        gtk_drop_down_set_model(GTK_DROP_DOWN(peer.drawing),
                                G_LIST_MODEL(model));
        g_object_unref(model);
        peer.updating = false;
        apply_selected_index();
    }
    void combo_box::apply_selected_index() {
        auto &peer = state(*this);
        peer.updating = true;
        gtk_drop_down_set_selected(
            GTK_DROP_DOWN(peer.drawing),
            get_selected_index() < 0
                ? GTK_INVALID_LIST_POSITION
                : static_cast<guint>(get_selected_index()));
        gtk_editable_set_text(GTK_EDITABLE(peer.editor),
                              get_text().c_str());
        peer.updating = false;
    }
    void combo_box::apply_text() {
        auto &peer = state(*this);
        peer.updating = true;
        gtk_editable_set_text(GTK_EDITABLE(peer.editor),
                              get_text().c_str());
        peer.updating = false;
    }
    void combo_box::apply_style() {
        gtk_widget_set_visible(state(*this).editor,
                               get_style() ==
                                   combo_box_style::editable);
    }
    void combo_box::show_native() {
        shown(*this);
    }
    void combo_box::destroy_native() {
        linux::wayland::destroy(*this);
    }
} // namespace native
