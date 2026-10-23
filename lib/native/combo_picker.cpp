//
// Composes a joined editable date/time field and a calendar from each
// toolkit's existing native buttons. Field arrows remain backend-owned;
// shared civil-date policy validates typed values and calendar choices.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "combo_picker.h"
#include "picker_peer.h"
#include "emulated_tree.h"
#include "wnd_peer.h"
#include <native.h>
#include <array>
#include <charconv>
#include <algorithm>
namespace
{
    using namespace native;
    class calendar_window final : public modeless_wnd
    {
    public:
        explicit calendar_window(app_wnd &owner)
            : modeless_wnd(owner, "Calendar") {}
        bool get_native_title_visible() const override {
            return false;
        }
    };
    class picker_field final : public detail::combo_picker_field
    {
    public:
    protected:
        void draw_indicator(gpx &graphics,
                            theme &appearance,
                            const rect &bounds,
                            const theme::state &state) override {
            const auto *actions =
                detail::peer_state<detail::combo_picker_actions>(*this);
            if (!actions || !actions->step) {
                combo_box::draw_indicator(
                    graphics, appearance, bounds, state);
                return;
            }
            auto saved = graphics.save_state();
            for (int part = 0; part < 2; ++part) {
                const int top = bounds.y1() + part * bounds.h() / 2;
                const int side = std::max(0, (int(bounds.h()) - 2) / 2);
                const rect button =
                    actions->square_stepper
                        ? rect(bounds.x2() - side - 1,
                               bounds.y1() + 1 + part * side,
                               side,
                               side)
                        : rect(bounds.x2() - bounds.h(),
                               top,
                               bounds.h(),
                               (part + 1) * bounds.h() / 2 -
                                   part * bounds.h() / 2);
                appearance.draw_button(button, "", state);
                if (actions->square_stepper) {
                    const int radius =
                        std::clamp(int(button.w()) / 4, 1, 3);
                    const int center =
                        button.x1() + (int(button.w()) - 1) / 2;
                    const int inset =
                        (int(button.h()) - radius - 1) / 2;
                    const int y =
                        button.y1() +
                        (part ? int(button.h()) - inset - radius - 1
                              : inset);
                    graphics.set_pen(1).set_ink(
                        appearance.native_palette().button_text);
                    for (int row = 0; row <= radius; ++row) {
                        const int half = part ? radius - row : row;
                        graphics.draw_line(
                            point(center - half, y + row),
                            point(center + half, y + row));
                    }
                    continue;
                }
                const int x = button.x1() + button.w() / 2,
                          y = button.y1() + button.h() / 2;
                graphics
                    .set_ink(appearance.native_palette().button_text)
                    .draw_polygon({point(x - 3, y + (part ? -1 : 1)),
                                   point(x + 3, y + (part ? -1 : 1)),
                                   point(x, y + (part ? 2 : -2))},
                                  true);
            }
        }
    };
    class day_button final : public button
    {
    public:
        bool enabled = false;
        day_button()
            : button("") {}
        bool get_input_enabled() const override {
            return enabled && button::get_input_enabled();
        }
    };
    struct picker
    {
        panel &owner;
        picker_field field;
        date shown;
        button previous{"<"}, next{">"};
        std::array<day_button, 42> days;
        std::unique_ptr<calendar_window> popup;
        int cell = 36, row = 28;
        explicit picker(panel &value)
            : owner(value) {
            field.set_parent(&owner);
        }
    };
    // Format one two-digit civil component.
    std::string two(unsigned value) {
        return (value < 10 ? "0" : "") + std::to_string(value);
    }
    // Keep dotted date text consistent across the classic adapters.
    std::string date_text(date value) {
        return two(value.day) + "." + two(value.month) + "." +
               std::to_string(value.year);
    }
    // Parse a complete unsigned component without accepting suffixes.
    bool number(std::string_view text, unsigned &value) {
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} &&
               result.ptr == text.data() + text.size();
    }
    // Apply typed proposals through the public availability policy.
    void commit(picker &state) {
        const std::string text = state.field.get_text();
        unsigned a = 0, b = 0, c = 0;
        if (auto *owner = dynamic_cast<date_picker *>(&state.owner)) {
            if (text.size() == 10 && text[2] == '.' && text[5] == '.' &&
                number(std::string_view(text).substr(0, 2), a) &&
                number(std::string_view(text).substr(3, 2), b) &&
                number(std::string_view(text).substr(6, 4), c))
                owner->on_native_date({int(c), b, a});
            else
                state.field.set_text(date_text(owner->get_date()));
        } else if (auto *owner =
                       dynamic_cast<time_picker *>(&state.owner)) {
            c = owner->get_time().second;
            if ((text.size() == 5 || text.size() == 8) &&
                text[2] == ':' &&
                number(std::string_view(text).substr(0, 2), a) &&
                number(std::string_view(text).substr(3, 2), b) &&
                (text.size() == 5 ||
                 (text[5] == ':' &&
                  number(std::string_view(text).substr(6, 2), c))))
                owner->on_native_time({a, b, c});
            else
                state.field.set_text(owner->get_time().to_string(
                    owner->get_seconds_visible()));
        }
    }
    // Synchronize native day captions and enabled states for one month.
    void refresh_calendar(picker &state) {
        auto &owner = static_cast<date_picker &>(state.owner);
        const int offset =
            int(date{state.shown.year, state.shown.month, 1}.weekday());
        for (int i = 0; i < 42; ++i) {
            const int day = i - offset + 1;
            const date value{state.shown.year,
                             state.shown.month,
                             unsigned(std::max(1, day))};
            state.days[i].set_text(
                day > 0 && value.valid() ? std::to_string(day) : "");
            state.days[i].enabled = day > 0 && value.valid() &&
                                    owner.is_date_enabled(value);
            if (state.days[i].get_created())
                detail::configure_picker_day(state.days[i],
                                             state.days[i].enabled);
        }
        state.popup->invalidate();
    }
    // Toggle a calendar, recreating its native children after
    // dismissal.
    void open(picker &state) {
        if (state.popup && state.popup->get_created()) {
            state.popup->destroy();
            return;
        }
        auto *owner =
            dynamic_cast<app_wnd *>(detail::root_of(&state.owner));
        if (!state.popup) {
            state.popup = std::make_unique<calendar_window>(*owner);
            const auto &font = font_t::stock(font_role::control);
            state.cell =
                std::max(36, font.measure_text("00").width + 18);
            state.row = std::max(26, font.get_metrics().height + 10);
            state.popup->set_dimensions(
                size(state.cell * 7 + 4, state.row * 8 + 4));
            state.popup->on_wnd_paint.connect(
                [&state](wnd_paint_event event) {
                    auto appearance = theme::create(event.g);
                    event.g
                        .clear(appearance->native_palette().button_bg)
                        .set_ink(
                            appearance->native_palette().button_text)
                        .set_font(font_t::stock(font_role::control));
                    const auto *actions = detail::peer_state<
                        detail::combo_picker_actions>(state.field);
                    if (actions && actions->framed_calendar)
                        appearance->draw_popup_frame(
                            state.popup->get_client_bounds());
                    event.g.draw_text(
                        std::to_string(state.shown.year) + "." +
                            two(state.shown.month),
                        rect(state.cell, 2, state.cell * 5, state.row),
                        {text_align::center, text_valign::center});
                    const char *names[] = {
                        "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
                    for (int i = 0; i < 7; ++i)
                        event.g.draw_text(
                            names[i],
                            rect(i * state.cell + 2,
                                 state.row + 2,
                                 state.cell,
                                 state.row),
                            {text_align::center, text_valign::center});
                    return true;
                });
            for (auto *control : {&state.previous, &state.next}) {
                control->set_parent(state.popup.get());
                control->set_bounds(rect(
                    control == &state.previous ? 2 : state.cell * 6 + 2,
                    2,
                    state.cell - 4,
                    state.row - 4));
                control->on_click.connect([&state, control] {
                    const int month = std::clamp(
                        state.shown.year * 12 + int(state.shown.month) -
                            1 + (control == &state.previous ? -1 : 1),
                        1601 * 12,
                        9999 * 12 + 11);
                    state.shown = {
                        month / 12, unsigned(month % 12 + 1), 1};
                    refresh_calendar(state);
                    return true;
                });
            }
            for (int i = 0; i < 42; ++i) {
                auto &control = state.days[i];
                control.set_parent(state.popup.get());
                control.set_bounds(rect((i % 7) * state.cell + 2,
                                        (i / 7 + 2) * state.row + 2,
                                        state.cell - 4,
                                        state.row - 4));
                control.on_click.connect([&state, i] {
                    const int offset =
                        int(date{state.shown.year, state.shown.month, 1}
                                .weekday());
                    const date value{state.shown.year,
                                     state.shown.month,
                                     unsigned(i - offset + 1)};
                    auto &owner =
                        static_cast<date_picker &>(state.owner);
                    if (owner.is_date_enabled(value)) {
                        state.popup->destroy();
                        owner.on_native_date(value);
                    }
                    return true;
                });
            }
        }
        state.popup->create();
        state.previous.create();
        state.previous.show();
        state.next.create();
        state.next.show();
        for (auto &day : state.days) {
            day.create();
            day.show();
        }
        state.shown =
            static_cast<date_picker &>(state.owner).get_date();
        refresh_calendar(state);
        state.popup->set_position(
            detail::picker_popup_position(state.field));
        state.popup->show();
    }
    // Attach field callbacks and state to their owning opaque peers.
    picker &create(panel &owner) {
        auto state = std::make_unique<picker>(owner);
        auto &actions = state->field.configured_actions();
        actions.commit = [state = state.get()] {
            commit(*state);
        };
        auto &result = *state;
        detail::assign_peer_state(owner, state.release());
        return result;
    }
} // namespace
namespace native::detail
{
    bool supports_combo_picker_actions() {
        return true;
    }
    combo_box &picker_field(panel &owner) {
        return peer_state<picker>(owner)->field;
    }
    app_wnd *picker_calendar(panel &owner) {
        return peer_state<picker>(owner)->popup.get();
    }
    bool create_date_picker_peer(date_picker &owner) {
        auto &state = create(owner);
        state.field.configured_actions().open = [&state] {
            open(state);
        };
        state.field.create();
        state.field.show();
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        auto &state = create(owner);
        state.field.configured_actions().step = [&owner, &state](
                                                    int direction) {
            const auto *actions =
                peer_state<combo_picker_actions>(state.field);
            const auto caret = actions->caret ? actions->caret() : 3;
            const int amount =
                caret < 3 ? 3600
                : caret < 6 || !owner.get_seconds_visible()
                    ? int(owner.get_minute_step()) * 60
                    : 1;
            const auto value = owner.get_time();
            const int seconds =
                std::clamp(int(value.hour * 3600 + value.minute * 60 +
                               value.second) +
                               direction * amount,
                           0,
                           86399);
            owner.on_native_time({unsigned(seconds / 3600),
                                  unsigned(seconds / 60 % 60),
                                  unsigned(seconds % 60)});
        };
        state.field.create();
        state.field.show();
        return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        if (auto *state = peer_state<picker>(owner)) {
            auto bounds = owner.get_client_bounds();
            const auto *actions =
                peer_state<combo_picker_actions>(state->field);
            if (actions && actions->compact_date)
                bounds.d.w = std::min<int>(
                    bounds.w(),
                    font_t::stock(font_role::control)
                            .measure_text("00.00.0000")
                            .width +
                        (actions->indicator_width
                             ? actions->indicator_width
                             : bounds.h()) +
                        (actions->text_margin
                             ? 2 * actions->text_margin + 2
                             : 12));
            state->field.set_bounds(bounds);
            state->field.set_text(date_text(owner.get_date()));
            if (state->popup && state->popup->get_created())
                refresh_calendar(*state);
        }
    }
    void refresh_time_picker_peer(time_picker &owner) {
        if (auto *state = peer_state<picker>(owner)) {
            auto bounds = owner.get_client_bounds();
            const auto text =
                owner.get_time().to_string(owner.get_seconds_visible());
            const auto *actions =
                peer_state<combo_picker_actions>(state->field);
            if (actions && actions->compact_time)
                bounds.d.w =
                    std::min<int>(bounds.w(),
                                  font_t::stock(font_role::control)
                                          .measure_text(text)
                                          .width +
                                      (actions->indicator_width
                                           ? actions->indicator_width
                                           : bounds.h() / 2) +
                                      2 * actions->text_margin + 2);
            state->field.set_bounds(bounds);
            state->field.set_text(text);
        }
    }
} // namespace native::detail
