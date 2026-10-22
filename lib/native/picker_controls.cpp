//
// Composes native combo boxes for toolkits without date/time widgets.
// Date choices omit unavailable days; all proposals use the shared rules.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_controls.h"
#include <algorithm>
#include <charconv>
#include "wnd_peer.h"

namespace
{
    std::vector<std::string> numbers(unsigned first, unsigned last, unsigned step = 1) {
        std::vector<std::string> result;
        for (unsigned value = first; value <= last; value += step)
            result.push_back(value < 10 ? "0" + std::to_string(value) : std::to_string(value));
        return result;
    }
    void create(native::wnd &control, native::panel &owner) {
        control.set_parent(&owner); control.create(); control.show();
    }
    void geometry(native::panel &owner, std::initializer_list<native::wnd *> controls) {
        const auto area = owner.get_client_bounds();
        const int count = controls.size();
        const int extent = std::max(0, int(area.w()) - (count - 1) * 4);
        int index = 0;
        for (auto *control : controls) {
            const int left = extent * index / count + index * 4;
            const int right = extent * (index + 1) / count + index * 4;
            control->set_bounds(native::rect(area.x1() + left, area.y1(), right - left, area.h()));
            ++index;
        }
    }
}
namespace native::detail
{
    picker_year_edit::picker_year_edit(date_picker &owner)
        : text_edit(std::to_string(owner.get_date().year)), _owner(owner) {
        set_validator([](const std::string &text) {
            return text.size() <= 4 && std::all_of(text.begin(), text.end(),
                [](char digit) { return digit >= '0' && digit <= '9'; });
        });
    }
    void picker_year_edit::on_native_focus(bool focused) {
        const window_guard guard(*this);
        text_edit::on_native_focus(focused);
        if (guard.alive() && !focused) set_text(std::to_string(_owner.get_date().year));
    }
    date_picker_controls::date_picker_controls(date_picker &owner) : _owner(owner), _year(owner) {
        _month.set_items(numbers(1, 12));
        for (auto *control : std::initializer_list<native::wnd *>{&_year, &_month, &_day}) create(*control, owner);
        _year.on_change.connect([this](const std::string &text) {
            if (text.size() != 4) return false;
            int year = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), year);
            return result.ec == std::errc{} && year >= 1601 && year <= 9999 && select(0, year);
        });
        _month.on_selection_change.connect([this](int index) { return select(1, index); });
        _day.on_selection_change.connect([this](int index) { return select(2, index); });
    }
    bool date_picker_controls::select(int field, int index) {
        if (index < 0) return false;
        auto value = _owner.get_date();
        if (field == 0) value.year = index;
        else if (field == 1) value.month = unsigned(index + 1);
        else {
            if (std::size_t(index) >= _days.size()) return false;
            value.day = _days[index];
        }
        if (field != 2 && !_owner.is_date_enabled(value)) {
            while (!value.valid() && value.day > 1) --value.day;
            const auto proposed = value;
            for (unsigned day = 1; day <= 31; ++day) {
                const native::date candidate{value.year, value.month, day};
                if (_owner.is_date_enabled(candidate)) { value = candidate; break; }
            }
            if (!_owner.is_date_enabled(value)) value = proposed;
        }
        return _owner.on_native_date(value);
    }
    void date_picker_controls::refresh() {
        const auto value = _owner.get_date();
        _year.set_text(std::to_string(value.year));
        _month.set_selected_index(int(value.month) - 1);
        _days.clear();
        std::vector<std::string> days;
        int selected = -1;
        for (unsigned day = 1; day <= 31; ++day) {
            if (!_owner.is_date_enabled({value.year, value.month, day})) continue;
            if (day == value.day) selected = int(days.size());
            _days.push_back(day); days.push_back(std::to_string(day));
        }
        _day.set_items(std::move(days)).set_selected_index(selected);
        geometry(_owner, {&_year, &_month, &_day});
    }
    time_picker_controls::time_picker_controls(time_picker &owner) : _owner(owner) {
        _hour.set_items(numbers(0, 23)); _second.set_items(numbers(0, 59));
        for (auto *control : {&_hour, &_minute, &_second}) create(*control, owner);
        _hour.on_selection_change.connect([this](int index) { return select(0, index); });
        _minute.on_selection_change.connect([this](int index) { return select(1, index); });
        _second.on_selection_change.connect([this](int index) { return select(2, index); });
    }
    bool time_picker_controls::select(int field, int index) {
        if (index < 0) return false;
        auto value = _owner.get_time();
        if (field == 0) value.hour = unsigned(index);
        else if (field == 1) value.minute = unsigned(index) * _owner.get_minute_step();
        else value.second = unsigned(index);
        return _owner.on_native_time(value);
    }
    void time_picker_controls::refresh() {
        const auto value = _owner.get_time();
        _hour.set_selected_index(value.hour);
        _minute.set_items(numbers(0, 59, _owner.get_minute_step()));
        _minute.set_selected_index(value.minute % _owner.get_minute_step() ? -1
            : int(value.minute / _owner.get_minute_step()));
        if (_owner.get_seconds_visible()) {
            if (!_second.get_created()) { _second.create(); _second.show(); }
            _second.set_selected_index(value.second);
            geometry(_owner, {&_hour, &_minute, &_second});
        } else {
            _second.destroy();
            geometry(_owner, {&_hour, &_minute});
        }
    }
}
