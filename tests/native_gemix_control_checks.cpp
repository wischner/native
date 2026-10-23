//
// Checks first-use GEM input and monochrome control pixels through the
// real Rasta/AES loop, including embedded fields, pickers and modals.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_gemix_control_checks.h"
#include "../lib/native/toolkits/gemix/globals.h"
#include "../lib/native/combo_picker.h"
#include "../lib/native/picker_peer.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
    using namespace native;
    using namespace linux::gemix;

    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    std::vector<unsigned char> frame() {
        std::ifstream file(std::getenv("GEM_RASTA_FRAMEBUFFER"),
                           std::ios::binary);
        expect(bool(file), "read GEM control framebuffer");
        return {std::istreambuf_iterator<char>(file), {}};
    }
    bool ink(const std::vector<unsigned char> &pixels, int x, int y) {
        if (pixels.size() == 904U * 900U * 4U) {
            const auto offset = (y * 904 + x) * 4;
            return int(pixels.at(offset)) + pixels.at(offset + 1) +
                       pixels.at(offset + 2) <
                   384;
        }
        expect(pixels.size() == 113U * 900U,
               "known GEM framebuffer format");
        return pixels.at(y * 113 + x / 8) & (0x80 >> (x % 8));
    }
    int count_ink(const std::vector<unsigned char> &pixels,
                  const rect &area) {
        int count = 0;
        for (int y = area.y1(); y < area.y2(); ++y)
            for (int x = area.x1(); x < area.x2(); ++x)
                count += ink(pixels, x, y);
        return count;
    }
    void attach(wnd &child, wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
} // namespace

void check_gemix_controls(native::app_wnd &owner,
                          const gem_control_driver &driver) {
    using namespace native;
    using namespace linux::gemix;
    modeless_wnd host(owner, "GEM control review", 40, 60, 720, 720);
    combo_box combo({"10 mm", "25 mm", "50 mm"},
                    combo_box_style::editable,
                    10,
                    10,
                    190,
                    30);
    date_picker dates(date{2026, 12, 24}, rect(240, 10, 200, 30));
    time_picker times(time_of_day{9, 30, 0}, rect(460, 10, 180, 30));
    tab_view tabs(10, 60, 340, 125);
    panel page, left, right, general;
    list choices({"One", "Two", "Three"}, 10, 10, 180, 70);
    split_view split(
        left, right, split_orientation::horizontal, 10, 210, 500, 70);
    settings_sidebar sidebar(10, 310, 340, 340);
    property_grid grid(380, 310, 310, 260);
    button command("Parent command", 380, 65, 170, 32);
    modal_wnd modal(host, "Modal paint", 0, 0, 240, 100);
    button modal_command("Modal command", 20, 20, 180, 32);
    point origin, combo_arrow, date_arrow, time_up, time_down, hour,
        minute, second, choice, divider, empty;
    rect normal_text, modal_text;
    const auto click = [&](point p) {
        driver.pointer(3, p);
        driver.pointer(10, p);
        driver.pointer(11, p);
    };
    const auto desktop = [&](wnd &control, int x, int y) {
        const auto bounds = root_bounds(control);
        return point(origin.x + bounds.x1() + x,
                     origin.y + bounds.y1() + y);
    };
    try {
        driver.wait(
            [&] {
                tabs.add_item("Choices", page);
                sidebar.set_sidebar_width(180).set_search_visible(true);
                sidebar << settings_page(
                    1, "General", general, {}, "Settings");
                times.set_seconds_visible(true).set_minute_step(15);
                grid.add_item({"name",
                               "Name",
                               property_kind::text,
                               std::string("Value"),
                               {},
                               false});
                grid.add_item({"choice",
                               "Choice",
                               property_kind::choice,
                               std::string("One"),
                               {"One", "Two"},
                               false});
                host.create();
                for (wnd *control : std::array<wnd *, 8>{&combo,
                                                         &dates,
                                                         &times,
                                                         &tabs,
                                                         &split,
                                                         &sidebar,
                                                         &grid,
                                                         &command})
                    attach(*control, host);
                attach(choices, page);
                host.show();
                origin =
                    work_rect(wnd_bindings.handle_from_object(&host)).p;
                combo_arrow =
                    desktop(combo, combo.get_dimensions().w - 5, 10);
                auto &date_field = detail::picker_field(dates);
                date_arrow = desktop(
                    date_field, date_field.get_dimensions().w - 5, 10);
                auto &time_field = detail::picker_field(times);
                for (auto *field : {&date_field, &time_field}) {
                    const int column =
                        field == &time_field
                            ? field->get_dimensions().h / 2
                            : field->get_dimensions().h;
                    const int expected =
                        font_t::stock(font_role::control)
                            .measure_text(field->get_text())
                            .width +
                        2 * std::max(1, int(runtime.char_w) / 2) +
                        column + 2;
                    expect(field->get_dimensions().w == expected,
                           "GEM picker fits text, half-character side "
                           "margins and its arrow column");
                }
                time_up = desktop(
                    time_field, time_field.get_dimensions().w - 5, 4);
                time_down = desktop(time_field,
                                    time_field.get_dimensions().w - 5,
                                    time_field.get_dimensions().h - 5);
                hour = desktop(time_field, 5, 10);
                minute =
                    desktop(time_field, 3 * runtime.char_w + 5, 10);
                second =
                    desktop(time_field, 6 * runtime.char_w + 5, 10);
                choice = desktop(choices, 25, 30);
                const auto handle = split.get_splitter_bounds();
                divider = desktop(split,
                                  handle.x1() + handle.w() / 2,
                                  handle.y1() + handle.h() / 2);
                empty = desktop(host, 690, 680);
                normal_text =
                    rect(origin.x + 390, origin.y + 72, 150, 18);
                flush_repaints();
                return true;
            },
            "create fresh GEM controls");

        click(combo_arrow);
        driver.wait(
            [&] {
                return combo_box_bindings.object_from_handle(&combo)
                    ->open;
            },
            "first combo arrow click opens its popup");
        click(combo_arrow);
        driver.wait(
            [&] {
                return !combo_box_bindings.object_from_handle(&combo)
                            ->open;
            },
            "second combo arrow click closes its popup");
        click(choice);
        driver.wait(
            [&] {
                return choices.get_selected_index() == 1;
            },
            "first click selects a list row inside a tab");
        click(date_arrow);
        driver.wait(
            [&] {
                const auto *popup = detail::picker_calendar(dates);
                return popup && popup->get_created();
            },
            "first date arrow click opens the calendar");
        click(date_arrow);
        driver.wait(
            [&] {
                return !detail::picker_calendar(dates)->get_created();
            },
            "second date arrow click closes the calendar");
        click(hour);
        click(time_up);
        driver.wait(
            [&] {
                return times.get_time() == time_of_day{10, 30, 0};
            },
            "time upper square steps the hour at the caret");
        click(minute);
        click(time_down);
        driver.wait(
            [&] {
                return times.get_time() == time_of_day{10, 15, 0};
            },
            "time lower square steps the selected minute");
        click(second);
        click(time_up);
        driver.wait(
            [&] {
                return times.get_time() == time_of_day{10, 15, 1};
            },
            "time square steps seconds at the caret");

        std::vector<unsigned char> arrow_frame;
        driver.pointer(3, empty);
        driver.wait(
            [&] {
                flush_repaints();
                arrow_frame = frame();
                expect(count_ink(
                           arrow_frame,
                           rect(empty.x - 8, empty.y - 8, 24, 28)) > 0,
                       "reference arrow cursor is visible in the Rasta "
                       "framebuffer");
                return true;
            },
            "record the standard arrow cursor");
        const point moved(divider.x + 70, divider.y);
        driver.pointer(3, divider);
        driver.pointer(10, divider);
        driver.pointer(3, moved);
        driver.pointer(11, moved);
        driver.pointer(3, empty);
        driver.wait(
            [&] {
                if (split.get_ratio() < 0.6f ||
                    window_states.object_from_handle(&host)->capture)
                    return false;
                flush_repaints();
                const auto restored = frame();
                for (int y = empty.y - 8; y < empty.y + 20; ++y)
                    for (int x = empty.x - 8; x < empty.x + 16; ++x)
                        expect(ink(restored, x, y) ==
                                   ink(arrow_frame, x, y),
                               "splitter release restores the actual "
                               "standard cursor bitmap");
                return true;
            },
            "release splitter capture and restore the arrow inside the "
            "window");

        driver.wait(
            [&] {
                // The VDI software cursor otherwise covers the last
                // arrow hit.
                v_hide_c(runtime.vdi_handle);
                flush_repaints();
                const auto pixels = frame();
                if (const char *path =
                        std::getenv("NATIVE_GEM_CONTROL_SNAPSHOT")) {
                    std::ofstream target(std::string(path) + ".fields",
                                         std::ios::binary);
                    target.write(
                        reinterpret_cast<const char *>(pixels.data()),
                        pixels.size());
                }
                for (auto *control : {&combo,
                                      &detail::picker_field(dates),
                                      &detail::picker_field(times)}) {
                    const auto bounds = root_bounds(*control);
                    for (int x = 3;
                         x < int(bounds.w()) - bounds.h() - 3;
                         ++x)
                        expect(ink(pixels,
                                   origin.x + bounds.x1() + x,
                                   origin.y + bounds.y1()) &&
                                   ink(pixels,
                                       origin.x + bounds.x1() + x,
                                       origin.y + bounds.y2() - 1),
                               "embedded combo/date/time editors "
                               "retain both frame edges");
                }
                auto &time_field = detail::picker_field(times);
                const auto bounds = root_bounds(time_field);
                const int side = (int(bounds.h()) - 2) / 2;
                const int x = origin.x + bounds.x2() - side - 1;
                const int y = origin.y + bounds.y1() + 1;
                expect(
                    combo_box_bindings.object_from_handle(&time_field)
                            ->editor->get_dimensions()
                            .w == bounds.w() - side - 2,
                    "time editor ends at the narrow square button "
                    "column");
                // Ignore button borders; compare only the centered
                // glyph area.
                for (int py = 3; py < side - 3; ++py)
                    for (int px = 2; px < side - 2; ++px)
                        expect(ink(pixels, x + px, y + py) ==
                                   ink(pixels,
                                       x + px,
                                       y + 2 * side - 1 - py),
                               "time arrows are the same mirrored "
                               "glyph inside equal squares");
                const auto navigation =
                    detail::settings_navigation_bounds(sidebar);
                const int height = theme::create(host.get_gpx())
                                       ->get_text_edit_height();
                expect(
                    navigation.x1() == 6 &&
                        navigation.y1() == height + 12 &&
                        navigation.y2() ==
                            sidebar.get_dimensions().h - 6,
                    "settings gap equals top, left and bottom margins");
                for (auto *tree : tree_views)
                    if (tree->get_parent() == &sidebar)
                        expect(tree->get_disclosure_bounds(0).x1() == 3,
                               "navigator headings retain only a small "
                               "inner left inset");
                const auto b = root_bounds(grid);
                const int row = grid.get_row_height();
                for (int py = row * 2 + 4; py < int(b.h()) - 5; ++py)
                    for (int px = int(b.w()) / 2 + 10;
                         px < int(b.w()) - 4;
                         ++px)
                        expect(!ink(pixels,
                                    origin.x + b.x1() + px,
                                    origin.y + b.y1() + py),
                               "value grid dots stop after the last "
                               "property");
                return true;
            },
            "GEM control pixels and navigation geometry");

        int enabled_ink = 0;
        driver.wait(
            [&] {
                enabled_ink = count_ink(frame(), normal_text);
                expect(enabled_ink > 0,
                       "enabled parent button has visible text");
                modal.create();
                attach(modal_command, modal);
                modal.show();
                host.invalidate();
                flush_repaints();
                const auto work =
                    work_rect(wnd_bindings.handle_from_object(&modal));
                modal_text =
                    rect(work.x1() + 25, work.y1() + 27, 170, 18);
                const auto pixels = frame();
                const int covered_ink = count_ink(pixels, normal_text);
                expect(covered_ink == enabled_ink &&
                           !command.get_input_enabled(),
                       "modal owner keeps normal button ink while "
                       "input remains blocked");
                expect(count_ink(pixels, modal_text) > 0,
                       "modal button text remains visible");
                if (const char *path =
                        std::getenv("NATIVE_GEM_CONTROL_SNAPSHOT")) {
                    std::ifstream source(
                        std::getenv("GEM_RASTA_FRAMEBUFFER"),
                        std::ios::binary);
                    std::ofstream target(path, std::ios::binary);
                    target << source.rdbuf();
                }
                modal.destroy();
                host.destroy();
                return true;
            },
            "modal labels use visible one-bit text");
    } catch (...) {
        driver.wait(
            [&] {
                modal.destroy();
                host.destroy();
                return true;
            },
            "release failed GEM controls on the UI thread");
        throw;
    }
}
