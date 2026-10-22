//
// Declares private native-widget adaptation hooks for settings and pickers.
// Their state belongs to the owner's existing window peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <native/settings_sidebar.h>

namespace native
{
    class date_picker;
    class time_picker;
    class settings_sidebar;
    struct time_of_day;
    namespace detail
    {
        // Close native islands before their containing panel HWND is removed.
        struct settings_sidebar_resources
        {
            virtual ~settings_sidebar_resources() = default;
            virtual void close() noexcept = 0;
        };
        // Return false when the backend composes its existing native controls.
        bool create_date_picker_peer(date_picker &owner);
        bool create_time_picker_peer(time_picker &owner);
        bool create_settings_sidebar_peer(settings_sidebar &owner);
        // Prefer native text lists when a toolkit has no native image tree.
        bool settings_sidebar_uses_list();
        // Dedicated adapters can provide a native search field themselves.
        bool settings_sidebar_uses_native_search();
        // Native framed navigation can reserve an outer panel margin.
        int settings_sidebar_navigation_padding();
        // Share filtering and navigation geometry across native adapters.
        std::vector<settings_page> settings_visible_items(const settings_sidebar &owner);
        rect settings_navigation_bounds(const settings_sidebar &owner);
        // Translate a native one-minute stepper move to the configured step.
        time_of_day stepped_time(const time_picker &owner, time_of_day proposal);
        // Apply model, validity and geometry without user notifications.
        void refresh_date_picker_peer(date_picker &owner);
        void refresh_time_picker_peer(time_picker &owner);
        void refresh_settings_sidebar_peer(settings_sidebar &owner);
    }
}
