//
// Hosts desktop-native GTK/Granite pickers in an ordinary panel peer.
// Widget factories and value conversion stay private to the desktop adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_widgets.h"
#include "../../picker_peer.h"
#include <charconv>

namespace linux::wayland
{
    GtkWidget *find_widget(GtkWidget *root, GType type) {
        if (G_TYPE_CHECK_INSTANCE_TYPE(root, type)) return root;
        for (auto *child = gtk_widget_get_first_child(root); child;
             child = gtk_widget_get_next_sibling(child))
            if (auto *found = find_widget(child, type)) return found;
        return nullptr;
    }
    void calendar_availability(native::date_picker &owner, picker_state &peer) {
        if (!peer.calendar) return;
        auto *calendar = GTK_CALENDAR(peer.calendar);
        auto *shown = gtk_calendar_get_date(calendar);
        const int year = g_date_time_get_year(shown);
        const unsigned month = g_date_time_get_month(shown);
        g_date_time_unref(shown);
        gtk_calendar_clear_marks(calendar);
        for (unsigned day = 1; day <= 31; ++day) {
            const native::date value{year, month, day};
            if (value.valid() && !owner.is_date_enabled(value)) gtk_calendar_mark_day(calendar, day);
        }
        gtk_widget_set_tooltip_text(peer.calendar, "Marked dates are unavailable.");
        std::vector<GtkWidget *> pending{peer.calendar};
        for (std::size_t index = 0; index < pending.size(); ++index) {
            auto *widget = pending[index];
            if (GTK_IS_LABEL(widget) && gtk_widget_has_css_class(widget, "day-number")) {
                const char *text = gtk_label_get_text(GTK_LABEL(widget));
                unsigned day = 0;
                std::from_chars(text, text + std::char_traits<char>::length(text), day);
                bool enabled = true;
                if (!gtk_widget_has_css_class(widget, "other-month") && day)
                    enabled = owner.is_date_enabled({year, month, day});
                gtk_widget_set_sensitive(widget, enabled);
                gtk_widget_set_tooltip_text(widget, enabled ? nullptr : "Unavailable date");
            }
            for (auto *child = gtk_widget_get_first_child(widget); child;
                 child = gtk_widget_get_next_sibling(child)) pending.push_back(child);
        }
    }
}
namespace
{
    using namespace linux::wayland;
    void place(native::wnd &owner, picker_state &value) {
        gtk_fixed_put(GTK_FIXED(state(owner).content), value.widget, 0, 0);
    }
    void geometry(native::wnd &owner, picker_state &value, bool valid) {
        const auto area = owner.get_client_bounds();
        int width = 0, height = 0;
        gtk_widget_set_size_request(value.widget, -1, area.h());
        gtk_widget_measure(value.widget, GTK_ORIENTATION_HORIZONTAL, -1,
                           nullptr, &width, nullptr, nullptr);
        width = std::min<int>(area.w(), width);
        gtk_widget_measure(value.widget, GTK_ORIENTATION_VERTICAL, width,
                           nullptr, &height, nullptr, nullptr);
        gtk_fixed_move(GTK_FIXED(state(owner).content), value.widget,
                       area.x1(), area.y1() + std::max(0, (int(area.h()) - height) / 2));
        if (valid) gtk_widget_remove_css_class(value.widget, "error");
        else gtk_widget_add_css_class(value.widget, "error");
        gtk_widget_set_tooltip_text(value.widget, valid ? nullptr : "The current value is unavailable.");
    }
}
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        auto *value = new linux::wayland::picker_state;
        assign_peer_state(owner, value);
        linux::wayland::date_widget(owner, *value); place(owner, *value); return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        auto *value = new linux::wayland::picker_state;
        assign_peer_state(owner, value);
        linux::wayland::time_widget(owner, *value); place(owner, *value); return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        auto *value = peer_state<linux::wayland::picker_state>(owner);
        if (!value || !value->widget) return;
        value->updating = true;
        linux::wayland::write_date(owner, *value);
        linux::wayland::calendar_availability(owner, *value);
        geometry(owner, *value, owner.get_valid()); value->updating = false;
    }
    void refresh_time_picker_peer(time_picker &owner) {
        auto *value = peer_state<linux::wayland::picker_state>(owner);
        if (!value || !value->widget) return;
        value->updating = true;
        linux::wayland::write_time(owner, *value);
        geometry(owner, *value, owner.get_valid()); value->updating = false;
    }
}
