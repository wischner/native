//
// Drives form navigation through Rasta packets and the real GEM event loop.
// Covers composite focus, wraparound and native-style control commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_gemix_control_checks.h"
#include "../lib/native/input_state.h"
#include "../lib/native/combo_picker.h"
#include "../lib/native/toolkits/gemix/globals.h"

#include <array>
#include <stdexcept>

namespace
{
    class focus_button final : public native::button
    {
    public:
        using native::button::button;
        bool painted_focus = false;
    protected:
        // Observe the real paint pass, not only the logical focus owner.
        void draw_focus(native::gpx &g, native::theme &appearance,
                        const native::rect &bounds,
                        const native::theme::state &state) override {
            painted_focus = state.focused;
            button::draw_focus(g, appearance, bounds, state);
        }
    };
}

void check_gemix_form(native::app_wnd &owner,
                      const gem_control_driver &driver) {
    using namespace native;
    using namespace linux::gemix;
    modeless_wnd host(owner, "Keyboard form", 50, 60, 580, 420);
    text_edit first("First", text_edit_mode::single_line, 20, 20, 300, 28);
    text_edit second("Second", text_edit_mode::single_line, 20, 62, 300, 28);
    combo_box city({"Ljubljana", "London", "Paris"},
                   combo_box_style::editable, 20, 104, 300, 28);
    date_picker date(native::date{2026, 11, 15}, rect(340, 20, 180, 24));
    time_picker time(time_of_day{9, 30, 0}, rect(340, 62, 180, 24));
    check option("Optional", 20, 170, 160, 28);
    button reset("Reset", 20, 220, 120, 28);
    focus_button last("Last", 160, 220, 120, 28);
    int toggles = 0, clicks = 0;
    option.on_change.connect([&](bool) { ++toggles; return true; });
    reset.on_click.connect([&] { ++clicks; return true; });
    auto key = [&](unsigned scan) {
        driver.key(scan, true); driver.key(scan, false);
    };
    auto focused = [&](wnd &control, const char *message) {
        driver.wait([&] { return detail::input_target(host) == &control; }, message);
    };
    auto combo_state = [&] { return combo_box_bindings.object_from_handle(&city); };
    try {
        driver.wait([&] {
            host.create();
            unsigned order = 0;
            for (wnd *control : std::array<wnd *, 8>{
                    &first, &second, &city, &date, &time, &option, &reset, &last}) {
                control->set_parent(&host).set_tab_order(++order);
                control->create(); control->show();
            }
            if (first.get_dimensions().h != 24 || city.get_dimensions().h != 24 ||
                reset.get_dimensions().h != 24 || last.get_dimensions().h != 24)
                throw std::runtime_error("GEM form inputs and default buttons share 24 pixels");
            if (combo_state()->editor->get_dimensions().h != 24)
                throw std::runtime_error("GEM combo editor fits its taller field");
            time.set_minute_step(15);
            if (date.get_dimensions().h != 24 || time.get_dimensions().h != 24 ||
                detail::picker_field(date).get_dimensions().h != 24 ||
                detail::picker_field(time).get_dimensions().h != 24)
                throw std::runtime_error("Standalone GEM pickers match text field height");
            city.set_selected_index(0);
            host.show(); first.focus();
            return true;
        }, "create keyboard form");
        focused(first, "initial form focus");
        key(40);
        focused(second, "single-line Enter moves to next field");
        key(43);
        driver.wait([&] {
            return detail::input_target(host) == combo_state()->editor.get();
        }, "Tab enters the combo's private editor");
        key(43);
        auto picker_editor = [&](panel &picker) {
            return combo_box_bindings.object_from_handle(&detail::picker_field(picker))->editor.get();
        };
        driver.wait([&] { return detail::input_target(host) == picker_editor(date); },
                    "Tab enters date picker editor");
        key(82);
        driver.wait([&] { return date.get_date() == native::date{2026, 11, 16}; },
                    "Up increments date component");
        key(81);
        driver.wait([&] { return date.get_date() == native::date{2026, 11, 15}; },
                    "Down decrements date component");
        key(61);
        driver.wait([&] {
            auto *popup = detail::picker_calendar(date);
            if (!popup || !popup->get_created()) return false;
            const auto handle = wnd_bindings.handle_from_object(popup);
            const auto outer = outer_rect(handle), work = work_rect(handle);
            if (work.x1() - outer.x1() != 1 || work.y1() - outer.y1() != 1 ||
                outer.w() - work.w() != 2 || outer.h() - work.h() != 2)
                throw std::runtime_error("GEM calendar must have a one-pixel normal frame");
            return true;
        }, "F4 opens normally bordered calendar");
        key(79); key(40);
        driver.wait([&] {
            return !detail::picker_calendar(date)->get_created() &&
                date.get_date() == native::date{2026, 11, 16} &&
                detail::input_target(host) == picker_editor(date);
        }, "Calendar arrows and Enter select a day and restore picker focus");
        key(61); key(81); key(41);
        driver.wait([&] {
            return !detail::picker_calendar(date)->get_created() &&
                date.get_date() == native::date{2026, 11, 16};
        }, "Calendar Escape cancels keyboard selection");
        driver.wait([&] {
            detail::picker_field(date).set_text("18.11.2026"); return true;
        }, "edit dotted date");
        key(40);
        driver.wait([&] { return date.get_date() == native::date{2026, 11, 18}; },
                    "Enter commits date text");
        key(43);
        driver.wait([&] { return detail::input_target(host) == picker_editor(time); },
                    "Tab enters time picker editor");
        key(82);
        driver.wait([&] { return time.get_time() == time_of_day{9, 45, 0}; },
                    "Up steps the time component");
        key(81);
        driver.wait([&] { return time.get_time() == time_of_day{9, 30, 0}; },
                    "Down restores time component");
        driver.wait([&] {
            detail::picker_field(time).set_text("11:15"); return true;
        }, "edit time text");
        key(40);
        driver.wait([&] { return time.get_time() == time_of_day{11, 15, 0}; },
                    "Enter commits time text");
        key(43);
        focused(option, "Tab leaves time picker for checkbox");
        key(43);
        focused(reset, "Tab reaches first button");
        key(43);
        focused(last, "Tab reaches final button");
        driver.wait([&] { flush_repaints(); return last.painted_focus; },
                    "last button paints its focus indicator");
        key(43);
        focused(first, "last-to-first Tab wrap");
        driver.wait([&] { flush_repaints(); return !last.painted_focus; },
                    "wrap removes the old button focus indicator");
        driver.key(225, true); key(43); driver.key(225, false);
        focused(last, "first-to-last Shift+Tab wrap");
        driver.wait([&] { option.focus(); return true; }, "focus checkbox");
        driver.key(44, true);
        driver.key(44, true); // Held-key repeat must not toggle twice.
        driver.key(44, false);
        driver.wait([&] { return option.get_checked() && toggles == 1; },
                    "Space checks exactly once");
        key(44);
        driver.wait([&] { return !option.get_checked() && toggles == 2; },
                    "Space unchecks exactly once");
        key(43); key(44);
        driver.wait([&] { return clicks == 1; }, "Space activates button");
        key(40);
        driver.wait([&] { return clicks == 2; }, "Enter activates button");
        driver.wait([&] { city.focus(); return true; }, "focus combo");
        key(61); // F4.
        driver.wait([&] { return combo_state()->open; }, "F4 opens editable combo");
        key(81); // Down.
        key(40);
        driver.wait([&] {
            return !combo_state()->open && city.get_selected_index() == 1 &&
                city.get_text() == "London" &&
                combo_state()->editor->get_text() == "London";
        }, "Down and Enter select popup item");
        driver.key(226, true); key(81); driver.key(226, false);
        driver.wait([&] { return combo_state()->open; }, "Alt+Down opens combo");
        key(81); key(41);
        driver.wait([&] {
            return !combo_state()->open && city.get_selected_index() == 1 &&
                city.get_text() == "London" &&
                combo_state()->editor->get_text() == "London";
        }, "Escape dismisses popup without changing selection");
        key(61); key(43);
        driver.wait([&] { return detail::input_target(host) == picker_editor(date); },
                    "Tab dismisses popup and traverses to date picker");
        driver.wait([&] { return !combo_state()->open; }, "Tab closes popup");
        point arrow;
        driver.wait([&] {
            const auto origin = work_rect(wnd_bindings.handle_from_object(&host)).p;
            const auto bounds = root_bounds(city);
            arrow = point(origin.x + bounds.x2() - 4,
                          origin.y + bounds.y1() + bounds.h() / 2);
            return true;
        }, "locate editable combo arrow");
        driver.pointer(3, arrow); driver.pointer(10, arrow); driver.pointer(11, arrow);
        driver.wait([&] { return combo_state()->open; }, "mouse opens editable combo");
        point choice;
        driver.wait([&] {
            const auto origin = work_rect(wnd_bindings.handle_from_object(&host)).p;
            const auto bounds = root_bounds(city);
            const int row = theme::create(host.get_gpx())->get_list_item_height();
            choice = point(origin.x + bounds.x1() + 20,
                           origin.y + bounds.y2() + 1 + 2 * row + row / 2);
            return true;
        }, "locate Paris in popup");
        driver.pointer(3, choice); driver.pointer(10, choice); driver.pointer(11, choice);
        driver.wait([&] {
            return !combo_state()->open && city.get_selected_index() == 2 &&
                city.get_text() == "Paris" && combo_state()->editor->get_text() == "Paris";
        }, "mouse selection updates both combo value and visible editor");
        driver.key(224, true); key(4); driver.key(224, false);
        key(4);
        driver.wait([&] { return city.get_text() == "a"; },
                    "mouse-opened combo retains editable text focus");
        driver.wait([&] { host.destroy(); return true; }, "close keyboard form");
    } catch (...) {
        driver.wait([&] { host.destroy(); return true; }, "release keyboard form");
        throw;
    }
}
