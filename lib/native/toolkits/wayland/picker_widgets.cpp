//
// Composes GTK pickers from linked native entries and menu buttons.
// GtkCalendar and GtkSpinButton own the popup content and input
// behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_widgets.h"
#include <charconv>

namespace
{
    bool number(std::string_view text, unsigned &value) {
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() &&
               result.ptr == text.data() + text.size();
    }
    GtkWidget *field(linux::wayland::picker_state &peer,
                     int characters) {
        peer.widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(peer.widget, "linked");
        peer.entry = linux::wayland::new_entry();
        gtk_editable_set_width_chars(GTK_EDITABLE(peer.entry),
                                     characters);
        gtk_widget_set_hexpand(peer.entry, true);
        gtk_box_append(GTK_BOX(peer.widget), peer.entry);
        auto *button = gtk_menu_button_new();
        gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button),
                                      "pan-down-symbolic");
        gtk_box_append(GTK_BOX(peer.widget), button);
        return button;
    }
} // namespace
namespace linux::wayland
{
    void date_widget(native::date_picker &owner, picker_state &peer) {
        auto *button = field(peer, 10);
        peer.calendar = gtk_calendar_new();
        auto *popover = gtk_popover_new();
        gtk_popover_set_child(GTK_POPOVER(popover), peer.calendar);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
        g_signal_connect(
            peer.entry,
            "activate",
            G_CALLBACK(+[](GtkEntry *entry, gpointer data) {
                auto &owner = *static_cast<native::date_picker *>(data);
                const std::string_view text =
                    gtk_editable_get_text(GTK_EDITABLE(entry));
                unsigned year = 0;
                unsigned month = 0;
                unsigned day = 0;
                const auto previous = owner.get_date();
                if (text.size() == 10 && text[4] == '-' &&
                    text[7] == '-' && number(text.substr(0, 4), year) &&
                    number(text.substr(5, 2), month) &&
                    number(text.substr(8, 2), day))
                    owner.on_native_date({int(year), month, day});
                else
                    owner.on_native_date(previous);
            }),
            &owner);
        g_signal_connect(
            peer.calendar,
            "day-selected",
            G_CALLBACK((+[](GtkCalendar *calendar, gpointer data) {
                auto &owner = *static_cast<native::date_picker *>(data);
                auto *peer =
                    native::detail::peer_state<picker_state>(owner);
                if (peer->updating)
                    return;
                auto *value = gtk_calendar_get_date(calendar);
                const native::date date{
                    g_date_time_get_year(value),
                    unsigned(g_date_time_get_month(value)),
                    unsigned(g_date_time_get_day_of_month(value))};
                g_date_time_unref(value);
                owner.on_native_date(date);
            })),
            &owner);
        for (auto signal : {"notify::month", "notify::year"})
            g_signal_connect(
                peer.calendar,
                signal,
                G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
                    auto &owner =
                        *static_cast<native::date_picker *>(data);
                    calendar_availability(
                        owner,
                        *native::detail::peer_state<picker_state>(
                            owner));
                }),
                &owner);
    }
    void write_date(native::date_picker &owner, picker_state &peer) {
        const auto value = owner.get_date();
        gtk_editable_set_text(GTK_EDITABLE(peer.entry),
                              value.to_string().c_str());
        auto *date = g_date_time_new_local(
            value.year, value.month, value.day, 12, 0, 0);
        gtk_calendar_select_day(GTK_CALENDAR(peer.calendar), date);
        g_date_time_unref(date);
    }
    void time_widget(native::time_picker &owner, picker_state &peer) {
        auto *button = field(peer, 8);
        auto *popover = gtk_popover_new();
        auto *fields = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_popover_set_child(GTK_POPOVER(popover), fields);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
        peer.hour = gtk_spin_button_new_with_range(0, 23, 1);
        peer.minute = gtk_spin_button_new_with_range(0, 59, 1);
        peer.second = gtk_spin_button_new_with_range(0, 59, 1);
        const char *names[] = {"Hour", "Minute", "Second"};
        int index = 0;
        for (auto *part : {peer.hour, peer.minute, peer.second}) {
            gtk_widget_set_tooltip_text(part, names[index++]);
            gtk_box_append(GTK_BOX(fields), part);
            g_signal_connect(
                part,
                "value-changed",
                G_CALLBACK(+[](GtkSpinButton *, gpointer data) {
                    auto &owner =
                        *static_cast<native::time_picker *>(data);
                    auto &peer =
                        *native::detail::peer_state<picker_state>(
                            owner);
                    if (!peer.updating)
                        owner.on_native_time(
                            {unsigned(gtk_spin_button_get_value_as_int(
                                 GTK_SPIN_BUTTON(peer.hour))),
                             unsigned(gtk_spin_button_get_value_as_int(
                                 GTK_SPIN_BUTTON(peer.minute))),
                             unsigned(gtk_spin_button_get_value_as_int(
                                 GTK_SPIN_BUTTON(peer.second)))});
                }),
                &owner);
        }
        g_signal_connect(
            peer.entry,
            "activate",
            G_CALLBACK(+[](GtkEntry *entry, gpointer data) {
                auto &owner = *static_cast<native::time_picker *>(data);
                const std::string_view text =
                    gtk_editable_get_text(GTK_EDITABLE(entry));
                unsigned hour = 0;
                unsigned minute = 0;
                unsigned second = owner.get_time().second;
                if ((text.size() == 5 || text.size() == 8) &&
                    text[2] == ':' && number(text.substr(0, 2), hour) &&
                    number(text.substr(3, 2), minute) &&
                    (text.size() == 5 ||
                     (text[5] == ':' &&
                      number(text.substr(6, 2), second))))
                    owner.on_native_time({hour, minute, second});
                else
                    owner.on_native_time(owner.get_time());
            }),
            &owner);
    }
    void write_time(native::time_picker &owner, picker_state &peer) {
        const auto value = owner.get_time();
        gtk_editable_set_text(
            GTK_EDITABLE(peer.entry),
            value.to_string(owner.get_seconds_visible()).c_str());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.hour),
                                  value.hour);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.minute),
                                  value.minute);
        gtk_spin_button_set_increments(GTK_SPIN_BUTTON(peer.minute),
                                       owner.get_minute_step(),
                                       owner.get_minute_step());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.second),
                                  value.second);
        gtk_widget_set_visible(peer.second,
                               owner.get_seconds_visible());
    }
} // namespace linux::wayland
