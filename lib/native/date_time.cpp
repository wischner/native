//
// Implements civil value validation, weekday calculation and local-clock
// snapshots using standard C++ facilities.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/date_time.h>
#include <array>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace native
{
    bool date::valid() const {
        if (year < 1601 || year > 9999 || month < 1 || month > 12 || day < 1) return false;
        constexpr std::array<unsigned, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        return day <= days[month - 1] + (month == 2 && leap ? 1U : 0U);
    }
    day_of_week date::weekday() const {
        if (!valid()) throw std::invalid_argument("Invalid civil date.");
        // Some maintained cross-toolchains lack chrono's civil calendar.
        // Count Gregorian days from 1 January 1, which was a Monday.
        constexpr std::array<unsigned, 12> before{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        const unsigned years = year - 1;
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        const unsigned days = years * 365 + years / 4 - years / 100 + years / 400 +
            before[month - 1] + day + (month > 2 && leap ? 1U : 0U);
        return static_cast<day_of_week>(days % 7);
    }
    date date::today() {
        const auto clock = std::time(nullptr);
        const auto *local = std::localtime(&clock);
        if (!local) throw std::runtime_error("Local date is unavailable.");
        return {local->tm_year + 1900, unsigned(local->tm_mon + 1), unsigned(local->tm_mday)};
    }
    std::string date::to_string() const {
        if (!valid()) throw std::invalid_argument("Invalid civil date.");
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setfill('0') << std::setw(4) << year << '-'
             << std::setw(2) << month << '-' << std::setw(2) << day;
        return text.str();
    }
    bool time_of_day::valid() const { return hour < 24 && minute < 60 && second < 60; }
    time_of_day time_of_day::now() {
        const auto clock = std::time(nullptr);
        const auto *local = std::localtime(&clock);
        if (!local) throw std::runtime_error("Local time is unavailable.");
        return {unsigned(local->tm_hour), unsigned(local->tm_min), unsigned(local->tm_sec)};
    }
    std::string time_of_day::to_string(bool seconds) const {
        if (!valid()) throw std::invalid_argument("Invalid wall-clock time.");
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setfill('0') << std::setw(2) << hour << ':' << std::setw(2) << minute;
        if (seconds) text << ':' << std::setw(2) << second;
        return text.str();
    }
}
