//
// Declares portable compositions of existing native choice controls for
// backends that have no public date or time picker widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/combo_box.h>
#include <native/date_picker.h>
#include <native/time_picker.h>
#include <native/text_edit.h>

namespace native::detail
{
    class picker_year_edit final : public text_edit
    {
    public:
        // Permit partial numeric entry without allocating thousands of rows.
        explicit picker_year_edit(date_picker &owner);
        // Restore an incomplete year when native focus leaves the field.
        void on_native_focus(bool focused) override;
    private:
        date_picker &_owner;
    };
    class date_picker_controls
    {
    public:
        explicit date_picker_controls(date_picker &owner);
        // Refresh choices, selected fields and logical geometry silently.
        void refresh();
    private:
        date_picker &_owner;
        picker_year_edit _year;
        combo_box _month, _day;
        std::vector<unsigned> _days;
        bool select(int field, int index);
    };
    class time_picker_controls
    {
    public:
        explicit time_picker_controls(time_picker &owner);
        // Refresh step choices, seconds visibility and geometry silently.
        void refresh();
    private:
        time_picker &_owner;
        combo_box _hour, _minute, _second;
        bool select(int field, int index);
    };
}
