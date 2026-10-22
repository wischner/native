//
// Implements wall-clock state and common acceptance rules independently
// of native time-control formats and operating-system date epochs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/time_picker.h>
#include <stdexcept>
#include "picker_controls.h"
#include "picker_peer.h"

#include "input_geometry.h"

namespace native
{
    rect time_picker::constrain_bounds(const rect &bounds) const {
        return detail::input_bounds(*this, bounds);
    }

    time_picker::time_picker(coord x, coord y, dim width, dim height)
        : time_picker(time_of_day::now(), rect(x, y, width, height)) { _time.second = 0; }
    time_picker::time_picker(const point &position, const size &dimensions)
        : time_picker(position.x, position.y, dimensions.w, dimensions.h) {}
    time_picker::time_picker(const rect &bounds) : time_picker(bounds.p, bounds.d) {}
    time_picker::time_picker(time_of_day value, const rect &bounds)
        : panel(bounds), _time(value) {
        if (!value.valid()) throw std::invalid_argument("Invalid initial time.");
    }
    time_picker::~time_picker() { destroy(); }
    time_of_day time_picker::get_time() const { return _time; }
    time_of_day time_picker::get_minimum_time() const { return _minimum; }
    time_of_day time_picker::get_maximum_time() const { return _maximum; }
    unsigned time_picker::get_minute_step() const { return _minute_step; }
    bool time_picker::get_seconds_visible() const { return _seconds_visible; }
    bool time_picker::get_valid() const { return is_time_enabled(_time); }
    bool time_picker::is_time_enabled(time_of_day value) const {
        if (!value.valid() || value < _minimum || value > _maximum ||
            value.minute % _minute_step != 0) return false;
        try { return !_validator || _validator(value); }
        catch (...) { return false; }
    }
    time_picker &time_picker::set_time(time_of_day value) {
        if (!is_time_enabled(value)) throw std::invalid_argument("Unavailable time.");
        _time = value; refresh(); return *this;
    }
    time_picker &time_picker::set_range(time_of_day first, time_of_day last) {
        if (!first.valid() || !last.valid() || last < first)
            throw std::invalid_argument("Invalid time range.");
        _minimum = first; _maximum = last; refresh(); return *this;
    }
    time_picker &time_picker::set_minute_step(unsigned step) {
        if (!step || step > 60 || 60 % step) throw std::invalid_argument("Invalid minute step.");
        _minute_step = step; refresh(); return *this;
    }
    time_picker &time_picker::set_seconds_visible(bool visible) {
        _seconds_visible = visible; refresh(); return *this;
    }
    time_picker &time_picker::set_validator(time_validator validator) {
        _validator = std::move(validator); refresh(); return *this;
    }
    bool time_picker::on_native_time(time_of_day value) {
        if (!get_input_enabled()) return false;
        if (!is_time_enabled(value)) {
            refresh(); on_rejected.emit(value); return false;
        }
        const bool changed = _time != value;
        _time = value; refresh();
        if (changed) on_change.emit(value);
        return true;
    }
    void time_picker::create_native() {
        panel::create_native();
        if (!detail::create_time_picker_peer(*this))
            _controls = std::make_unique<detail::time_picker_controls>(*this);
        refresh();
    }
    void time_picker::destroy_native() {
        destroy_children(); _controls.reset(); panel::destroy_native();
    }
    void time_picker::on_bounds_changed() { refresh(); }
    void time_picker::refresh() {
        if (!get_created()) return;
        if (_controls) _controls->refresh();
        else detail::refresh_time_picker_peer(*this);
    }
}

namespace native::detail
{
    time_of_day stepped_time(const time_picker &owner, time_of_day proposal) {
        const auto current = owner.get_time();
        const int step = owner.get_minute_step();
        if (!proposal.valid() || proposal.minute % step == 0 || proposal.second != current.second)
            return proposal;
        const int before = int(current.hour * 60 + current.minute);
        const int after = int(proposal.hour * 60 + proposal.minute);
        const int delta = after - before;
        int minutes;
        if (delta == 1 || delta == -1439) minutes = (before / step + 1) * step;
        else if (delta == -1 || delta == 1439) minutes = before ? (before - 1) / step * step : 1440 - step;
        else return proposal;
        minutes %= 1440;
        return {unsigned(minutes / 60), unsigned(minutes % 60), proposal.second};
    }
}
