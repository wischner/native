//
// Exercises settings-page builders, native navigation, shared icon artwork,
// icons, holiday/weekend exclusions, time steps and seconds visibility.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_settings.h"
#include "navigation_icons.h"
#include <algorithm>

namespace
{
    void show_child(native::wnd &control, native::wnd &parent) {
        control.set_parent(&parent); control.create(); control.show();
    }
    void bounds(native::wnd &control, int x, int y, int width, int height) {
        control.set_bounds(native::rect(x, y, std::max(0, width), height));
    }
}
namespace vision
{
    feature_settings::feature_settings(native::app_wnd &owner)
        : modeless_wnd(owner, "Vision Settings and Pickers", 70, 65, 760, 430),
          _notes("Settings navigation\n\nSelect Dates or Time in the sidebar.\n"
                 "Pages use stable IDs and optional Native images.\n"
                 "Changing pages preserves their C++ values.\n\n"
                 "The date page demonstrates application-defined holidays,\n"
                 "weekends, date ranges and explicit exclusions.\n"
                 "The time page demonstrates minute steps and seconds.\n\n"
                 "Icons: Font Awesome Free 6.7.2 / Fonticons, Inc.\n"
                 "CC BY 4.0 — https://fontawesome.com/license/free",
                 native::text_edit_mode::multi_line),
          _date_help("Choose an available date, then try the exclusion rules.\n"
                     "Unavailable selections are rejected.\n"
                     "Changing rules preserves the current value and reports validity.", native::text_edit_mode::multi_line),
          _time_help("Pick a time between 08:00 and 18:00.\n"
                     "Try a fifteen-minute step, then enable seconds.", native::text_edit_mode::multi_line),
          _time(native::time_of_day{9, 30, 0}, native::rect(0, 0, 240, 32)),
          _weekends("Exclude Saturdays and Sundays"),
          _holidays("Exclude New Year's Day and Christmas"),
          _seconds("Show seconds"), _quarters("Fifteen-minute steps"),
          _exclude_date("Exclude this date"), _clear_dates("Clear excluded dates"),
          _sidebar(0, 0, 760, 430) {
        _sidebar.set_parent(this);
        _sidebar.set_search_visible(true);
        for (auto *text : {&_notes, &_date_help, &_time_help, &_date_status, &_time_status})
            text->set_read_only(true);
        const auto year = native::date::today().year;
        _date.set_range({year, 1, 1}, {std::min(9999, year + 2), 12, 31});
        _time.set_range({8, 0, 0}, {18, 0, 0});
        _general.on_wnd_create.connect([this] { show_child(_notes, _general); return true; });
        _dates.on_wnd_create.connect([this] {
            for (native::wnd *control : std::initializer_list<native::wnd *>{&_date_help, &_date,
                    &_weekends, &_holidays, &_exclude_date, &_clear_dates, &_date_status})
                show_child(*control, _dates);
            date_status("Date"); layout(); return true;
        });
        _times.on_wnd_create.connect([this] {
            for (native::wnd *control : std::initializer_list<native::wnd *>{&_time_help, &_time,
                    &_seconds, &_quarters, &_time_status}) show_child(*control, _times);
            time_status("Time"); layout(); return true;
        });
        for (auto *page : {&_general, &_dates, &_times})
            page->on_wnd_resize.connect([this](native::size) { layout(); return true; });
        _weekends.on_change.connect([this](bool checked) {
            _date.set_disabled_weekdays(checked ? std::vector<native::day_of_week>{
                native::day_of_week::saturday, native::day_of_week::sunday} : std::vector<native::day_of_week>{});
            date_status("Rules changed"); return true;
        });
        _holidays.on_change.connect([this](bool checked) {
            _date.set_validator(checked ? native::date_validator([](native::date value) {
                return !(value.month == 1 && value.day == 1) && !(value.month == 12 && value.day == 25);
            }) : native::date_validator{});
            date_status("Rules changed"); return true;
        });
        _exclude_date.on_click.connect([this] {
            auto days = _date.get_disabled_dates(); days.push_back(_date.get_date());
            _date.set_disabled_dates(std::move(days)); date_status("Date excluded"); return true;
        });
        _clear_dates.on_click.connect([this] {
            _date.set_disabled_dates({}); date_status("Exclusions cleared"); return true;
        });
        _seconds.on_change.connect([this](bool checked) {
            _time.set_seconds_visible(checked); time_status("Display changed"); return true;
        });
        _quarters.on_change.connect([this](bool checked) {
            _time.set_minute_step(checked ? 15 : 1); time_status("Rules changed"); return true;
        });
        _date.on_change.connect([this](native::date) { date_status("Accepted"); return true; });
        _date.on_rejected.connect([this](native::date value) {
            _date_status.set_text("Unavailable: " + value.to_string()); return true;
        });
        _time.on_change.connect([this](native::time_of_day) { time_status("Accepted"); return true; });
        _time.on_rejected.connect([this](native::time_of_day value) {
            _time_status.set_text("Unavailable: " + value.to_string()); return true;
        });
        on_wnd_create.connect(this, &feature_settings::on_create);
        on_wnd_resize.connect([this](native::size) { layout(); return true; });
    }
    bool feature_settings::on_create() {
        if (_sidebar.get_items().empty()) {
            _sidebar << native::settings_page(1, "General", _general,
                         navigation_image(navigation_icon::settings), "Settings")
                     << native::settings_page(2, "Dates", _dates,
                         navigation_image(navigation_icon::calendar), "Controls")
                     << native::settings_page(3, "Time", _times,
                         navigation_image(navigation_icon::clock), "Controls");
        }
        layout(); _sidebar.create(); _sidebar.show(); layout(); return true;
    }
    void feature_settings::layout() {
        _sidebar.set_bounds(get_client_bounds());
        const int width = std::max(0, int(_sidebar.get_content_bounds().w()) - 32);
        bounds(_notes, 16, 16, width, 270);
        bounds(_date_help, 16, 16, width, 76); bounds(_date, 16, 108, std::min(340, width), 34);
        bounds(_weekends, 16, 154, width, 26); bounds(_holidays, 16, 188, width, 26);
        bounds(_exclude_date, 16, 228, 170, 30); bounds(_clear_dates, 200, 228, 210, 30);
        bounds(_date_status, 16, 282, width, 30);
        bounds(_time_help, 16, 16, width, 66); bounds(_time, 16, 104, std::min(300, width), 34);
        bounds(_seconds, 16, 154, width, 26); bounds(_quarters, 16, 188, width, 26);
        bounds(_time_status, 16, 246, width, 30);
    }
    void feature_settings::date_status(const std::string &prefix) {
        _date_status.set_text(prefix + ": " + _date.get_date().to_string() +
            (_date.get_valid() ? " (available)" : " (unavailable under current rules)"));
    }
    void feature_settings::time_status(const std::string &prefix) {
        _time_status.set_text(prefix + ": " + _time.get_time().to_string() +
            (_time.get_valid() ? " (available)" : " (unavailable under current rules)"));
    }
}
