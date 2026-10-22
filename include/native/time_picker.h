//
// Declares a native-first wall-clock picker with portable range, step,
// seconds visibility, and application availability validation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <functional>
#include <memory>
#include "date_time.h"
#include "panel.h"

namespace native
{
    namespace detail { class time_picker_controls; }
    using time_validator = std::function<bool(time_of_day)>;

    class time_picker : public panel
    {
    public:
        // Construct with the local time (whole minutes) and scalar bounds.
        time_picker(coord x = 0, coord y = 0, dim width = 160, dim height = 24);
        // Construct with local whole minutes, position and dimensions.
        time_picker(const point &position, const size &dimensions);
        // Construct with local whole minutes and complete bounds.
        explicit time_picker(const rect &bounds);
        // Construct with an explicit initial time and complete bounds.
        time_picker(time_of_day value, const rect &bounds);
        // Release the native control and its composed children.
        ~time_picker() override;
        // Return the complete cached wall-clock time.
        time_of_day get_time() const;
        // Set an available time silently; throw invalid_argument otherwise.
        time_picker &set_time(time_of_day value);
        // Set inclusive limits in one day; preserve the value and emit nothing.
        time_picker &set_range(time_of_day first, time_of_day last);
        // Return the inclusive lower limit.
        time_of_day get_minimum_time() const;
        // Return the inclusive upper limit.
        time_of_day get_maximum_time() const;
        // Set a minute step dividing 60; preserve the current time silently.
        time_picker &set_minute_step(unsigned step);
        // Return the minute step (default one).
        unsigned get_minute_step() const;
        // Show or hide seconds without discarding the cached seconds.
        time_picker &set_seconds_visible(bool visible);
        // Return whether seconds can be edited (default false).
        bool get_seconds_visible() const;
        // Install an availability predicate; empty permits all other times.
        time_picker &set_validator(time_validator validator);
        // Test range, minute step and predicate without changing the control.
        bool is_time_enabled(time_of_day value) const;
        // Return whether the cached time satisfies the current rules.
        bool get_valid() const;
        // Accept a user time, or restore the native value and return false.
        virtual bool on_native_time(time_of_day value);
        // Emits only after a distinct, accepted user selection.
        signal<time_of_day> on_change;
        // Emits a rejected user proposal without changing the cached time.
        signal<time_of_day> on_rejected;

    protected:
        // Use the active theme's standard single-line field height.
        rect constrain_bounds(const rect &bounds) const override;

        // Create the platform picker or compose existing native choices.
        void create_native() override;
        // Destroy native resources while preserving the model.
        void destroy_native() override;
        // Resize and synchronize after a geometry change.
        void on_bounds_changed() override;

    private:
        time_of_day _time;
        time_of_day _minimum{0, 0, 0}, _maximum{23, 59, 59};
        unsigned _minute_step = 1;
        bool _seconds_visible = false;
        time_validator _validator;
        std::unique_ptr<detail::time_picker_controls> _controls;
        void refresh();
    };
}
