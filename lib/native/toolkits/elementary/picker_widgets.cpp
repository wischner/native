//
// Uses Granite's date and time pickers, retaining Native's availability
// policy. An optional GTK seconds field extends Granite's minute editor.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../wayland/picker_widgets.h"
#include <granite-7.h>
#include "../../picker_peer.h"
#include <algorithm>

namespace linux::wayland
{
    void date_widget(native::date_picker &owner, picker_state &peer) {
        peer.widget = GTK_WIDGET(granite_date_picker_new()); peer.entry = peer.widget;
        gtk_editable_set_editable(GTK_EDITABLE(peer.entry), true);
        gtk_widget_set_tooltip_text(peer.entry, "Enter a date and press Enter, or open the calendar.");
        g_signal_connect(peer.entry, "activate", G_CALLBACK(+[](GtkEntry *entry, gpointer data) {
            auto &owner = *static_cast<native::date_picker *>(data);
            GDate parsed{};
            g_date_set_parse(&parsed, gtk_editable_get_text(GTK_EDITABLE(entry)));
            if (!g_date_valid(&parsed)) {
                gtk_widget_add_css_class(GTK_WIDGET(entry), "error");
                return;
            }
            owner.on_native_date({int(g_date_get_year(&parsed)),
                unsigned(g_date_get_month(&parsed)), unsigned(g_date_get_day(&parsed))});
        }), &owner);
        peer.calendar = find_widget(peer.widget, GTK_TYPE_CALENDAR);
        g_signal_connect(peer.widget, "notify::date", G_CALLBACK(+[](GObject *widget, GParamSpec *, gpointer data) {
            auto &owner = *static_cast<native::date_picker *>(data);
            if (native::detail::peer_state<picker_state>(owner)->updating) return;
            auto *date = granite_date_picker_get_date(GRANITE_DATE_PICKER(widget));
            owner.on_native_date({g_date_time_get_year(date), unsigned(g_date_time_get_month(date)),
                unsigned(g_date_time_get_day_of_month(date))});
        }), &owner);
        if (peer.calendar) {
            for (auto signal : {"notify::month", "notify::year"})
                g_signal_connect(peer.calendar, signal, G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
                    auto &owner = *static_cast<native::date_picker *>(data);
                    calendar_availability(owner, *native::detail::peer_state<picker_state>(owner));
                }), &owner);
        }
    }
    void write_date(native::date_picker &owner, picker_state &peer) {
        const auto value = owner.get_date();
        auto *current = granite_date_picker_get_date(GRANITE_DATE_PICKER(peer.widget));
        // GObject can queue a property notification until the current notify
        // handler returns. A boolean guard alone cannot suppress that replay.
        if (current && g_date_time_get_year(current) == value.year &&
            unsigned(g_date_time_get_month(current)) == value.month &&
            unsigned(g_date_time_get_day_of_month(current)) == value.day) {
            char *text = g_date_time_format(current, granite_date_picker_get_format(GRANITE_DATE_PICKER(peer.widget)));
            gtk_editable_set_text(GTK_EDITABLE(peer.entry), text);
            gtk_editable_set_width_chars(GTK_EDITABLE(peer.entry),
                std::max(10, int(g_utf8_strlen(text, -1))));
            g_free(text);
            return;
        }
        auto *date = g_date_time_new_local(value.year, value.month, value.day, 12, 0, 0);
        granite_date_picker_set_date(GRANITE_DATE_PICKER(peer.widget), date);
        char *text = g_date_time_format(date, granite_date_picker_get_format(GRANITE_DATE_PICKER(peer.widget)));
        gtk_editable_set_text(GTK_EDITABLE(peer.entry), text);
        gtk_editable_set_width_chars(GTK_EDITABLE(peer.entry), std::max(10, int(g_utf8_strlen(text, -1))));
        g_free(text); g_date_time_unref(date);
    }
    void time_widget(native::time_picker &owner, picker_state &peer) {
        peer.widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        peer.entry = GTK_WIDGET(granite_time_picker_new());
        peer.second = gtk_spin_button_new_with_range(0, 59, 1);
        gtk_widget_set_tooltip_text(peer.second, "Second");
        gtk_widget_set_hexpand(peer.entry, true);
        gtk_box_append(GTK_BOX(peer.widget), peer.entry);
        gtk_box_append(GTK_BOX(peer.widget), peer.second);
        g_signal_connect(peer.entry, "time-changed", G_CALLBACK(+[](GraniteTimePicker *widget, gpointer data) {
            auto &owner = *static_cast<native::time_picker *>(data);
            auto &peer = *native::detail::peer_state<picker_state>(owner);
            if (peer.updating) return;
            auto *time = granite_time_picker_get_time(widget);
            owner.on_native_time(native::detail::stepped_time(owner,
                {unsigned(g_date_time_get_hour(time)), unsigned(g_date_time_get_minute(time)), owner.get_time().second}));
        }), &owner);
        g_signal_connect(peer.second, "value-changed", G_CALLBACK(+[](GtkSpinButton *field, gpointer data) {
            auto &owner = *static_cast<native::time_picker *>(data);
            auto &peer = *native::detail::peer_state<picker_state>(owner);
            if (peer.updating) return;
            auto value = owner.get_time(); value.second = gtk_spin_button_get_value_as_int(field);
            owner.on_native_time(value);
        }), &owner);
    }
    void write_time(native::time_picker &owner, picker_state &peer) {
        gtk_editable_set_width_chars(GTK_EDITABLE(peer.entry), 5);
        const auto value = owner.get_time();
        auto *current = granite_time_picker_get_time(GRANITE_TIME_PICKER(peer.entry));
        if (!current || unsigned(g_date_time_get_hour(current)) != value.hour ||
            unsigned(g_date_time_get_minute(current)) != value.minute ||
            unsigned(g_date_time_get_second(current)) != value.second) {
            auto *time = g_date_time_new_local(2000, 1, 1, value.hour, value.minute, value.second);
            granite_time_picker_set_time(GRANITE_TIME_PICKER(peer.entry), time); g_date_time_unref(time);
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.second), value.second);
        gtk_widget_set_visible(peer.second, owner.get_seconds_visible());
    }
}
