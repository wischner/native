//
// Declares Vision's settings navigation and date/time acceptance gallery.
// The application uses only Native's public API and ordinary image assets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>

namespace vision
{
    class feature_settings final : public native::modeless_wnd
    {
    public:
        // Construct the modeless gallery, borrowing its application owner.
        explicit feature_settings(native::app_wnd &owner);
    private:
        native::panel _general, _dates, _times;
        native::text_edit _notes, _date_help, _time_help;
        native::date_picker _date;
        native::time_picker _time;
        native::check _weekends, _holidays, _seconds, _quarters;
        native::button _exclude_date, _clear_dates;
        native::text_edit _date_status, _time_status;
        // Borrowed pages outlive the sidebar because it is destroyed first.
        native::settings_sidebar _sidebar;
        bool on_create();
        void layout();
        void date_status(const std::string &prefix);
        void time_status(const std::string &prefix);
    };
}
