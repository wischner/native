//
// Shares private GTK picker state with the Granite specialization.
// Desktop factories provide native widgets; the portable model owns rules.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "globals.h"

namespace linux::wayland
{
    struct picker_state
    {
        GtkWidget *widget = nullptr;
        GtkWidget *entry = nullptr;
        GtkWidget *calendar = nullptr;
        GtkWidget *hour = nullptr;
        GtkWidget *minute = nullptr;
        GtkWidget *second = nullptr;
        bool updating = false;
    };
    // Create the desktop-native date/time widgets inside the existing panel.
    void date_widget(native::date_picker &owner, picker_state &peer);
    void time_widget(native::time_picker &owner, picker_state &peer);
    // Apply values and availability without calling user notification hooks.
    void write_date(native::date_picker &owner, picker_state &peer);
    void write_time(native::time_picker &owner, picker_state &peer);
    // Locate a native child by type (including popover children).
    GtkWidget *find_widget(GtkWidget *root, GType type);
    // Mark unavailable days in the visible calendar and explain the marks.
    void calendar_availability(native::date_picker &owner, picker_state &peer);
}
