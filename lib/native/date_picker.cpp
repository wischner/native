//
// Implements the portable date model and availability policy. All native
// pickers and composed choices pass proposals through the same validator.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/date_picker.h>
#include <algorithm>
#include <stdexcept>
#include "picker_controls.h"
#include "picker_peer.h"

#include "input_geometry.h"

namespace native
{
    rect date_picker::constrain_bounds(const rect &bounds) const {
        return detail::input_bounds(*this, bounds);
    }

    date_picker::date_picker(coord x, coord y, dim width, dim height)
        : date_picker(date::today(), rect(x, y, width, height)) {}
    date_picker::date_picker(const point &position, const size &dimensions)
        : date_picker(date::today(), rect(position, dimensions)) {}
    date_picker::date_picker(const rect &bounds) : date_picker(date::today(), bounds) {}
    date_picker::date_picker(date value, const rect &bounds)
        : panel(bounds), _date(value) {
        if (!value.valid()) throw std::invalid_argument("Invalid initial date.");
    }
    date_picker::~date_picker() { destroy(); }
    date date_picker::get_date() const { return _date; }
    date date_picker::get_minimum_date() const { return _minimum; }
    date date_picker::get_maximum_date() const { return _maximum; }
    const std::vector<date> &date_picker::get_disabled_dates() const { return _disabled_dates; }
    const std::vector<day_of_week> &date_picker::get_disabled_weekdays() const { return _disabled_weekdays; }
    bool date_picker::get_valid() const { return is_date_enabled(_date); }
    bool date_picker::is_date_enabled(date value) const {
        if (!value.valid() || value < _minimum || value > _maximum ||
            std::binary_search(_disabled_dates.begin(), _disabled_dates.end(), value) ||
            std::find(_disabled_weekdays.begin(), _disabled_weekdays.end(), value.weekday())
                != _disabled_weekdays.end()) return false;
        // Application exceptions must never unwind through a native callback.
        try { return !_validator || _validator(value); }
        catch (...) { return false; }
    }
    date_picker &date_picker::set_date(date value) {
        if (!is_date_enabled(value)) throw std::invalid_argument("Unavailable date.");
        _date = value;
        refresh();
        return *this;
    }
    date_picker &date_picker::set_range(date first, date last) {
        if (!first.valid() || !last.valid() || last < first)
            throw std::invalid_argument("Invalid date range.");
        _minimum = first; _maximum = last;
        refresh();
        return *this;
    }
    date_picker &date_picker::set_disabled_dates(std::vector<date> dates) {
        for (auto value : dates)
            if (!value.valid()) throw std::invalid_argument("Invalid excluded date.");
        std::sort(dates.begin(), dates.end());
        dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
        _disabled_dates = std::move(dates);
        refresh();
        return *this;
    }
    date_picker &date_picker::set_disabled_weekdays(std::vector<day_of_week> days) {
        for (auto day : days)
            if (unsigned(day) > 6) throw std::invalid_argument("Invalid weekday.");
        std::sort(days.begin(), days.end());
        days.erase(std::unique(days.begin(), days.end()), days.end());
        _disabled_weekdays = std::move(days);
        refresh();
        return *this;
    }
    date_picker &date_picker::set_validator(date_validator validator) {
        _validator = std::move(validator);
        refresh();
        return *this;
    }
    bool date_picker::on_native_date(date value) {
        if (!get_input_enabled()) return false;
        if (!is_date_enabled(value)) {
            refresh();
            on_rejected.emit(value);
            return false;
        }
        const bool changed = _date != value;
        _date = value;
        refresh();
        if (changed) on_change.emit(value);
        return true;
    }
    void date_picker::create_native() {
        panel::create_native();
        if (!detail::create_date_picker_peer(*this))
            _controls = std::make_unique<detail::date_picker_controls>(*this);
        refresh();
    }
    void date_picker::destroy_native() {
        destroy_children();
        _controls.reset();
        panel::destroy_native();
    }
    void date_picker::on_bounds_changed() { refresh(); }
    void date_picker::refresh() {
        if (!get_created()) return;
        if (_controls) _controls->refresh();
        else detail::refresh_date_picker_peer(*this);
    }
}
