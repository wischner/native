//
// Provides GTK calendar/popover and spin-button picker widgets on Wayland.
// No calendar cells or time fields are painted by the library.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_widgets.h"

namespace linux::wayland
{
    void date_widget(native::date_picker &owner, picker_state &peer) {
        peer.entry = gtk_label_new(nullptr);
        peer.calendar = gtk_calendar_new();
        auto *popover = gtk_popover_new();
        gtk_popover_set_child(GTK_POPOVER(popover), peer.calendar);
        peer.widget = gtk_menu_button_new();
        gtk_menu_button_set_child(GTK_MENU_BUTTON(peer.widget), peer.entry);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(peer.widget), popover);
        g_signal_connect(peer.calendar, "day-selected", G_CALLBACK((+[](GtkCalendar *calendar, gpointer data) {
            auto &owner = *static_cast<native::date_picker *>(data);
            auto *peer = native::detail::peer_state<picker_state>(owner);
            if (peer->updating) return;
            auto *value = gtk_calendar_get_date(calendar);
            const native::date date{g_date_time_get_year(value),
                unsigned(g_date_time_get_month(value)), unsigned(g_date_time_get_day_of_month(value))};
            g_date_time_unref(value); owner.on_native_date(date);
        })), &owner);
        for (auto signal : {"notify::month", "notify::year"})
            g_signal_connect(peer.calendar, signal, G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
                auto &owner = *static_cast<native::date_picker *>(data);
                calendar_availability(owner, *native::detail::peer_state<picker_state>(owner));
            }), &owner);
    }
    void write_date(native::date_picker &owner, picker_state &peer) {
        const auto value = owner.get_date();
        gtk_label_set_text(GTK_LABEL(peer.entry), value.to_string().c_str());
        auto *date = g_date_time_new_local(value.year, value.month, value.day, 12, 0, 0);
        gtk_calendar_select_day(GTK_CALENDAR(peer.calendar), date); g_date_time_unref(date);
    }
    void time_widget(native::time_picker &owner, picker_state &peer) {
        peer.widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        peer.hour = gtk_spin_button_new_with_range(0, 23, 1);
        peer.minute = gtk_spin_button_new_with_range(0, 59, 1);
        peer.second = gtk_spin_button_new_with_range(0, 59, 1);
        const char *names[] = {"Hour", "Minute", "Second"};
        int index = 0;
        for (auto *field : {peer.hour, peer.minute, peer.second}) {
            gtk_widget_set_tooltip_text(field, names[index++]);
            gtk_widget_set_hexpand(field, true);
            gtk_box_append(GTK_BOX(peer.widget), field);
            g_signal_connect(field, "value-changed", G_CALLBACK(+[](GtkSpinButton *, gpointer data) {
                auto &owner = *static_cast<native::time_picker *>(data);
                auto &peer = *native::detail::peer_state<picker_state>(owner);
                if (!peer.updating) owner.on_native_time({unsigned(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(peer.hour))),
                    unsigned(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(peer.minute))),
                    unsigned(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(peer.second)))});
            }), &owner);
        }
    }
    void write_time(native::time_picker &owner, picker_state &peer) {
        const auto value = owner.get_time();
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.hour), value.hour);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.minute), value.minute);
        gtk_spin_button_set_increments(GTK_SPIN_BUTTON(peer.minute), owner.get_minute_step(), owner.get_minute_step());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.second), value.second);
        gtk_widget_set_visible(peer.second, owner.get_seconds_visible());
    }
}
