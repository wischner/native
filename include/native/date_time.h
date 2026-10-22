//
// Declares civil Gregorian dates and wall-clock times without a time zone.
// These values are independent of operating-system date representations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <compare>
#include <string>

namespace native
{
    // Identifies Gregorian weekdays independently of locale or OS numbering.
    enum class day_of_week { sunday, monday, tuesday, wednesday,
                             thursday, friday, saturday };

    struct date
    {
        int year = 2000;
        unsigned month = 1;
        unsigned day = 1;

        // Return whether this is a real date in the portable 1601..9999 range.
        bool valid() const;
        // Return the weekday; throw invalid_argument for an invalid date.
        day_of_week weekday() const;
        // Return the local calendar date reported by the system clock.
        static date today();
        // Return an unambiguous YYYY-MM-DD representation.
        std::string to_string() const;
        // Compare civil fields chronologically.
        auto operator<=>(const date &) const = default;
    };

    struct time_of_day
    {
        unsigned hour = 0;
        unsigned minute = 0;
        unsigned second = 0;

        // Return whether fields are within 00:00:00..23:59:59.
        bool valid() const;
        // Return the local wall-clock time reported by the system clock.
        static time_of_day now();
        // Return a 24-hour HH:MM or HH:MM:SS representation.
        std::string to_string(bool seconds = true) const;
        // Compare wall-clock fields chronologically.
        auto operator<=>(const time_of_day &) const = default;
    };
}
