//
// Extends the SDL editable combo field with a themed calendar popup
// or paired time-step arrows. Shared date/time policies validate every
// typed value and popup choice; no extra top-level window is needed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include <charconv>

namespace
{
    using namespace native;
    using namespace linux::sdl2;

    struct picker_state
    {
        panel &owner;
        combo_box field{{}, combo_box_style::editable};
        date shown;
        explicit picker_state(panel &value)
            : owner(value) {
            field.set_parent(&owner);
            field.create();
            field.show();
        }
    };

    bool number(std::string_view text, unsigned &value) {
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() &&
               result.ptr == text.data() + text.size();
    }

    // Keep the calendar aligned with the combo, flipping above when
    // needed.
    rect popup_bounds(picker_state &state) {
        const auto bounds = root_bounds(state.field);
        const auto available = root_of(&state.field)->get_dimensions();
        const int row = theme::create(root_of(&state.owner)->get_gpx())
                            ->get_list_item_height();
        const int width =
            std::min<int>(available.w, std::max(224, int(bounds.w())));
        const int height = row * 8 + 2;
        const int top = bounds.y2() + height <= available.h
                            ? bounds.y2()
                            : bounds.y1() - height;
        return rect(
            std::clamp<int>(
                bounds.x1(), 0, std::max(0, int(available.w) - width)),
            std::max(0, top),
            width,
            height);
    }

    void close(picker_state &state) {
        auto *peer =
            combo_box_bindings.object_from_handle(&state.field);
        peer->open = false;
        state.field.on_native_drop_down(false);
        state.owner.invalidate();
    }

    // Geometry is identical for drawing and pointer selection.
    rect day_bounds(const rect &popup, int row, int index) {
        const int column = index % 7;
        const int width = int(popup.w()) - 2;
        const int x1 = popup.x1() + 1 + column * width / 7;
        const int x2 = popup.x1() + 1 + (column + 1) * width / 7;
        return rect(
            x1, popup.y1() + 1 + (index / 7 + 2) * row, x2 - x1, row);
    }

    void paint_calendar(picker_state &state, gpx &graphics) {
        auto saved = graphics.save_state();
        auto appearance = theme::create(graphics);
        const auto popup = popup_bounds(state);
        graphics.set_clip(graphics.get_clip().intersect(popup));
        const int row = appearance->get_list_item_height();
        const auto colors = appearance->native_palette();
        appearance->draw_popup_frame(popup);
        appearance->draw_button(
            rect(popup.x1() + 1, popup.y1() + 1, row, row), "<");
        appearance->draw_button(
            rect(popup.x2() - row - 1, popup.y1() + 1, row, row), ">");
        const auto title = std::to_string(state.shown.year) + "-" +
                           (state.shown.month < 10 ? "0" : "") +
                           std::to_string(state.shown.month);
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(colors.content_text)
            .draw_text(title,
                       rect(popup.x1() + row + 1,
                            popup.y1() + 1,
                            std::max(1, int(popup.w()) - 2 * row - 2),
                            row),
                       {text_align::center, text_valign::center});
        const char *names[] = {
            "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        for (int i = 0; i < 7; ++i) {
            auto cell = day_bounds(popup, row, i);
            cell.p.y -= row;
            graphics.draw_text(
                names[i],
                cell,
                {text_align::center, text_valign::center});
        }
        auto &owner = static_cast<date_picker &>(state.owner);
        const int offset =
            int(date{state.shown.year, state.shown.month, 1}.weekday());
        for (int i = 0; i < 42; ++i) {
            const int day = i - offset + 1;
            const date value{state.shown.year,
                             state.shown.month,
                             unsigned(std::max(1, day))};
            if (day < 1 || !value.valid())
                continue;
            theme::state button;
            button.disabled = !owner.is_date_enabled(value);
            button.selected = value == owner.get_date();
            const auto cell = day_bounds(popup, row, i);
            appearance->draw_list_item(cell, "", button);
            graphics
                .set_ink(button.disabled   ? colors.button_disabled_text
                         : button.selected ? colors.selection_text
                                           : colors.content_text)
                .draw_text(std::to_string(day),
                           cell,
                           {text_align::center, text_valign::center});
        }
    }

    bool click_calendar(picker_state &state, int x, int y) {
        const auto popup = popup_bounds(state);
        const point position(x, y);
        const int row = theme::create(root_of(&state.owner)->get_gpx())
                            ->get_list_item_height();
        if (!popup.contains(position)) {
            close(state);
            return true;
        }
        if (y < popup.y1() + row + 1) {
            int delta = x < popup.x1() + row + 1    ? -1
                        : x >= popup.x2() - row - 1 ? 1
                                                    : 0;
            const int month =
                std::clamp(state.shown.year * 12 +
                               int(state.shown.month) - 1 + delta,
                           1601 * 12,
                           9999 * 12 + 11);
            state.shown = {month / 12, unsigned(month % 12 + 1), 1};
            state.owner.invalidate();
            return true;
        }
        auto &owner = static_cast<date_picker &>(state.owner);
        const int offset =
            int(date{state.shown.year, state.shown.month, 1}.weekday());
        for (int i = 0; i < 42; ++i) {
            if (!day_bounds(popup, row, i).contains(position))
                continue;
            const int day = i - offset + 1;
            if (day < 1)
                return true;
            const date value{
                state.shown.year, state.shown.month, unsigned(day)};
            if (!owner.is_date_enabled(value))
                return true;
            close(state);
            owner.on_native_date(value);
            return true;
        }
        return true;
    }

    void commit(picker_state &state) {
        const std::string text = state.field.get_text();
        unsigned first = 0, second = 0, third = 0;
        if (auto *owner = dynamic_cast<date_picker *>(&state.owner)) {
            if (text.size() == 10 && text[4] == '-' && text[7] == '-' &&
                number(std::string_view(text).substr(0, 4), first) &&
                number(std::string_view(text).substr(5, 2), second) &&
                number(std::string_view(text).substr(8, 2), third))
                owner->on_native_date({int(first), second, third});
            else
                owner->on_native_date(owner->get_date());
        } else if (auto *owner =
                       dynamic_cast<time_picker *>(&state.owner)) {
            third = owner->get_time().second;
            if ((text.size() == 5 || text.size() == 8) &&
                text[2] == ':' &&
                number(std::string_view(text).substr(0, 2), first) &&
                number(std::string_view(text).substr(3, 2), second) &&
                (text.size() == 5 ||
                 (text[5] == ':' &&
                  number(std::string_view(text).substr(6, 2), third))))
                owner->on_native_time({first, second, third});
            else
                owner->on_native_time(owner->get_time());
        }
    }

    void step(picker_state &state, int direction) {
        auto &owner = static_cast<time_picker &>(state.owner);
        const auto *field =
            combo_box_bindings.object_from_handle(&state.field);
        const auto *editor =
            text_edit_bindings.object_from_handle(field->editor.get());
        const auto value = owner.get_time();
        const auto cursor = editor ? editor->cursor : 3;
        const int amount = cursor < 3 ? 3600
                           : cursor < 6
                               ? int(owner.get_minute_step()) * 60
                               : 1;
        const int seconds = std::clamp(
            int(value.hour * 3600 + value.minute * 60 + value.second) +
                direction * amount,
            0,
            86399);
        owner.on_native_time({unsigned(seconds / 3600),
                              unsigned(seconds / 60 % 60),
                              unsigned(seconds % 60)});
    }

    picker_state &create(panel &owner) {
        auto state = std::make_unique<picker_state>(owner);
        auto *peer =
            combo_box_bindings.object_from_handle(&state->field);
        peer->commit = [value = state.get()] {
            commit(*value);
        };
        auto &result = *state;
        detail::assign_peer_state(owner, state.release());
        return result;
    }

    void refresh(picker_state &state, const std::string &text) {
        const auto bounds = state.owner.get_client_bounds();
        const auto current = state.field.get_bounds();
        if (current.x1() != bounds.x1() ||
            current.y1() != bounds.y1() || current.w() != bounds.w() ||
            current.h() != bounds.h())
            state.field.set_bounds(bounds);
        state.field.set_text(text);
        state.owner.invalidate();
    }
} // namespace
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        auto &state = create(owner);
        auto *peer = linux::sdl2::combo_box_bindings.object_from_handle(
            &state.field);
        peer->popup_paint = [&state](gpx &graphics) {
            paint_calendar(state, graphics);
        };
        peer->popup_click = [&state](int x, int y) {
            return click_calendar(state, x, y);
        };
        state.field.on_drop_down.connect([&state](bool open) {
            if (open)
                state.shown =
                    static_cast<date_picker &>(state.owner).get_date();
            return true;
        });
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        auto &state = create(owner);
        auto *peer = linux::sdl2::combo_box_bindings.object_from_handle(
            &state.field);
        peer->step = [&state](int direction) {
            step(state, direction);
        };
        return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        if (auto *state = peer_state<picker_state>(owner))
            refresh(*state, owner.get_date().to_string());
    }
    void refresh_time_picker_peer(time_picker &owner) {
        if (auto *state = peer_state<picker_state>(owner))
            refresh(*state,
                    owner.get_time().to_string(
                        owner.get_seconds_visible()));
    }
} // namespace native::detail
