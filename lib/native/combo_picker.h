//
// Declares private composition hooks for pickers that extend an
// existing editable combo. Native adapters retain ownership of the
// field and arrows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <functional>
#include <cstddef>
#include <native/geometry.h>
#include <native/combo_box.h>
namespace native
{
    class combo_box;
    class button;
    class panel;
    class app_wnd;
    namespace detail
    {
        struct combo_picker_actions
        {
            std::function<void()> open;
            std::function<void()> commit;
            std::function<void(int)> step;
            // Optional native caret/selection position for component
            // stepping.
            std::function<std::size_t()> caret;
            // Painted adapters may use a narrow column of square
            // halves.
            bool square_stepper = false;
            // Classic native fields can fit their date text and frame
            // the calendar independently of the enclosing page width.
            bool compact_date = false;
            bool compact_time = false;
            // Horizontal editor padding in pixels, excluding its frame.
            int text_margin = 0;
            // Native arrow column width when it differs from the
            // default full-height date / half-height time indicator.
            int indicator_width = 0;
            bool framed_calendar = false;
            bool read_only = false;
        };
        // Prepare composition callbacks before the backend creates
        // widgets.
        class combo_picker_field : public combo_box
        {
        public:
            // Construct an empty editable field for a private
            // composition.
            combo_picker_field();
            // Configure callbacks before create(); native peers get a
            // copy.
            combo_picker_actions &configured_actions();

        protected:
            // Publish callbacks after peer allocation, before native
            // creation.
            void create_native() override;

        private:
            combo_picker_actions _actions;
        };
        // Report whether native combo arrows accept these private
        // callbacks.
        bool supports_combo_picker_actions();
        // Locate the field's lower left corner in desktop coordinates.
        point picker_popup_position(combo_box &field);
        // Apply availability to an existing native day button.
        void configure_picker_day(button &day, bool enabled);
        // Inspect the existing native field or the lazily created
        // calendar.
        combo_box &picker_field(panel &owner);
        app_wnd *picker_calendar(panel &owner);
    } // namespace detail
} // namespace native
