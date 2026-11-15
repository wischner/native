//
// Declares Vision's interactive text editing and tab traversal form.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <native.h>

namespace vision
{
    class feature_edit_form final : public native::modeless_wnd
    {
    public:
        // Construct the form with its owning gallery window.
        explicit feature_edit_form(native::app_wnd &owner);

        // Destroy native children before callback state disappears.
        ~feature_edit_form() override;

        // Create or reopen the form and focus the first field.
        void open();

    private:
        native::text_edit _first, _last, _quantity, _optional;
        native::text_edit _enter_off, _notes, _skipped;
        native::combo_box _city;
        native::date_picker _date;
        native::time_picker _time;
        native::check _enable_optional;
        native::button _reset, _close;
        std::string _feedback;
        int _form_height = 0;

        // Locate rows using the active theme's ordinary or compact form pitch.
        int row_y(unsigned row) const;

        // Restore sample values and the initial optional-field state.
        void reset();

        // Lay out the fields inside the current client width.
        void layout();

        // Paint field labels, traversal instructions and edit feedback.
        void paint(native::wnd_paint_event event);
    };
}
