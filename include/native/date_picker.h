//
// Declares a native-first date picker with shared availability rules.
// Applications supply holiday lists or predicates; no locale is assumed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "date_time.h"
#include "panel.h"

namespace native
{
    namespace detail { class date_picker_controls; }
    using date_validator = std::function<bool(date)>;

    class date_picker : public panel
    {
    public:
        // Construct with today's local date and scalar bounds.
        date_picker(coord x = 0, coord y = 0, dim width = 220, dim height = 24);
        // Construct with today's local date, position and dimensions.
        date_picker(const point &position, const size &dimensions);
        // Construct with today's local date and complete bounds.
        explicit date_picker(const rect &bounds);
        // Construct with an explicit initial date and complete bounds.
        date_picker(date value, const rect &bounds);
        // Release the native control and its composed children.
        ~date_picker() override;
        // Return the cached date, including after a rule makes it unavailable.
        date get_date() const;
        // Set an available date silently; throw invalid_argument otherwise.
        date_picker &set_date(date value);
        // Set inclusive valid limits; preserve the current date and emit nothing.
        date_picker &set_range(date first, date last);
        // Return the inclusive lower limit.
        date get_minimum_date() const;
        // Return the inclusive upper limit.
        date get_maximum_date() const;
        // Replace the excluded dates after validating every date.
        date_picker &set_disabled_dates(std::vector<date> dates);
        // Return the sorted, unique excluded dates.
        const std::vector<date> &get_disabled_dates() const;
        // Replace excluded weekdays (for example Saturday and Sunday).
        date_picker &set_disabled_weekdays(std::vector<day_of_week> days);
        // Return the excluded weekdays.
        const std::vector<day_of_week> &get_disabled_weekdays() const;
        // Install a pure availability predicate; empty permits all other dates.
        date_picker &set_validator(date_validator validator);
        // Test all rules without changing the control or emitting signals.
        bool is_date_enabled(date value) const;
        // Return whether the cached date satisfies the current rules.
        bool get_valid() const;
        // Accept a user date, or restore the native value and return false.
        virtual bool on_native_date(date value);
        // Emits only after a distinct, accepted user selection.
        signal<date> on_change;
        // Emits an unavailable user proposal, without changing the cached date.
        signal<date> on_rejected;

    protected:
        // Use the active theme's standard single-line field height.
        rect constrain_bounds(const rect &bounds) const override;

        // Create the platform picker or compose existing native choices.
        void create_native() override;
        // Destroy resources while preserving values and availability rules.
        void destroy_native() override;
        // Resize and synchronize the picker after a geometry change.
        void on_bounds_changed() override;

    private:
        date _date;
        date _minimum{1601, 1, 1}, _maximum{9999, 12, 31};
        std::vector<date> _disabled_dates;
        std::vector<day_of_week> _disabled_weekdays;
        date_validator _validator;
        std::unique_ptr<detail::date_picker_controls> _controls;
        void refresh();
    };
}
