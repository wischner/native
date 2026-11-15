//
// Demonstrates explicit keyboard traversal and contrasting Enter behavior
// with native text fields, a multiline editor and an editable combo.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_edit_form.h"

#include <algorithm>
#include <array>

namespace vision
{
    feature_edit_form::feature_edit_form(native::app_wnd &owner)
        : modeless_wnd(owner, "Vision Edit Form", 80, 60, 780, 730),
          _notes({}, native::text_edit_mode::multi_line),
          _city({"Ljubljana", "London", "Paris"},
                native::combo_box_style::editable),
          _enable_optional("10. Include optional field"),
          _reset("11. Reset form"), _close("12. Close") {
        _first.set_tab_order(1);
        _last.set_tab_order(2);
        _quantity.set_tab_order(3);
        _optional.set_tab_order(4);
        _enter_off.set_tab_order(5);
        _enter_off.set_enter_as_tab(false);
        _notes.set_tab_order(6);
        _city.set_tab_order(7);
        _date.set_tab_order(8);
        _time.set_tab_order(9);
        _enable_optional.set_tab_order(10);
        _reset.set_tab_order(11);
        _close.set_tab_order(12);
        _quantity.set_validator([](const std::string &text) {
            return std::all_of(text.begin(), text.end(), [](char c) {
                return c >= '0' && c <= '9';
            });
        });
        reset();
        on_wnd_create.connect([this] {
            _form_height = native::theme::create(get_gpx())->get_form_control_height();
            set_dimensions(native::size(get_dimensions().w, row_y(11) + 90));
            layout();
            for (native::wnd *control : std::array<native::wnd *, 13>{
                    &_first, &_last, &_quantity, &_optional, &_enter_off,
                    &_notes, &_city, &_date, &_time, &_skipped, &_enable_optional,
                    &_reset, &_close}) {
                control->set_parent(this);
                control->create();
                control->show();
            }
            layout();
            return true;
        });
        on_wnd_resize.connect([this](native::size) {
            layout(); return true;
        });
        on_wnd_paint.connect([this](native::wnd_paint_event event) {
            paint(event); return true;
        });
        for (auto *field : {&_first, &_last, &_quantity, &_optional,
                            &_enter_off, &_notes, &_skipped}) {
            field->on_change.connect([this, field](const std::string &text) {
                _feedback = "Edited field (order " +
                    std::to_string(field->get_tab_order()) + "): " +
                    std::to_string(text.size()) + " UTF-8 bytes";
                invalidate();
                return true;
            });
        }
        _date.on_change.connect([this](native::date value) {
            _feedback = "Date: " + value.to_string(); invalidate(); return true;
        });
        _time.on_change.connect([this](native::time_of_day value) {
            _feedback = "Time: " + value.to_string(); invalidate(); return true;
        });
        _enable_optional.on_change.connect([this](bool enabled) {
            _optional.set_read_only(!enabled);
            _optional.set_tab_order(enabled ? 4 : 0);
            _feedback = enabled ? "Optional field joins traversal at 4."
                                : "Optional field is read-only and skipped.";
            invalidate();
            return true;
        });
        _reset.on_click.connect([this] {
            reset(); _first.focus(); return true;
        });
        _close.on_click.connect([this] { destroy(); return true; });
    }

    feature_edit_form::~feature_edit_form() { destroy(); }

    void feature_edit_form::open() {
        if (!get_created()) create();
        show();
        _first.focus();
    }

    void feature_edit_form::reset() {
        _first.set_text("Ada");
        _last.set_text("Lovelace");
        _quantity.set_text("12");
        _optional.set_text("Include me using the checkbox below");
        _optional.set_read_only(true);
        _optional.set_tab_order(0);
        _enable_optional.set_checked(false);
        _enter_off.set_text("Enter stays here; Tab moves on");
        _notes.set_text("Enter adds a new line here.\nTab moves to the city field.");
        _city.set_selected_index(0);
        _date.set_date(native::date::today());
        _time.set_time({9, 30, 0});
        _skipped.set_text("Click to edit me; tab order is 0");
        _feedback = "Ready. Try editing, selecting, and copying/pasting text.";
        if (get_created()) invalidate();
    }

    void feature_edit_form::layout() {
        const auto area = get_client_bounds();
        const int x = area.x1() + 226;
        const int width = std::max(120, int(area.w()) - 246);
        const int top = area.y1();
        const int height = _form_height > 0 ? _form_height : 30;
        const std::array<native::wnd *, 5> fields{
            &_first, &_last, &_quantity, &_optional, &_enter_off};
        for (std::size_t i = 0; i < fields.size(); ++i)
            fields[i]->set_bounds(native::rect(x, top + row_y(i), width, height));
        _notes.set_bounds(native::rect(x, top + row_y(5), width, 82));
        _city.set_bounds(native::rect(x, top + row_y(6), width, height));
        _date.set_bounds(native::rect(x, top + row_y(7), width, height));
        _time.set_bounds(native::rect(x, top + row_y(8), width, height));
        _skipped.set_bounds(native::rect(x, top + row_y(9), width, height));
        _enable_optional.set_bounds(native::rect(x, top + row_y(10), width, _form_height > 0 ? _form_height : 28));
        const int button_height = _form_height > 0 ? _form_height : 32;
        _reset.set_bounds(native::rect(x, top + row_y(11), 145, button_height));
        _close.set_bounds(native::rect(x + 157, top + row_y(11), 125, button_height));
    }

    int feature_edit_form::row_y(unsigned row) const {
        const int top = _form_height > 0 ? 74 : 86;
        const int pitch = _form_height > 0 ? 32 : 42;
        if (row <= 5) return top + int(row) * pitch;
        return top + 5 * pitch + 82 + (_form_height > 0 ? 6 : 12) +
            int(row - 6) * pitch;
    }

    void feature_edit_form::paint(native::wnd_paint_event event) {
        const auto area = get_client_bounds();
        auto saved = event.g.save_state();
        event.g.set_ink(native::theme::create(event.g)->native_palette().content_text);
        event.g.set_font(native::font_t::stock(native::font_role::control));
        auto line = [&](const std::string &text, int y, int width) {
            event.g.draw_text(text,
                native::rect(area.x1() + 16, area.y1() + y, width,
                             _form_height > 0 ? _form_height : 28),
                {native::text_align::start, native::text_valign::center,
                 native::text_overflow::ellipsis, true});
        };
        const int width = std::max(1, int(area.w()) - 32);
        line("Tab: next field. Shift+Tab: previous. Traversal wraps at both ends.",
             10, width);
        line("Single-line Enter moves next by default; Shift+Enter moves back.",
             38, width);
        const std::array<const char *, 5> labels{
            "1. First name", "2. Last name", "3. Quantity (digits only)",
            "4. Optional (starts skipped)", "5. Enter-as-Tab OFF"};
        for (std::size_t i = 0; i < labels.size(); ++i)
            line(labels[i], row_y(i), 200);
        line("6. Notes (multiline)", row_y(5), 200);
        line("7. City (editable combo)", row_y(6), 200);
        line("8. Date", row_y(7), 200);
        line("9. Time", row_y(8), 200);
        line("Skipped (no tab order)", row_y(9), 200);
        line(_feedback, row_y(12) + 4, width);
    }
}
