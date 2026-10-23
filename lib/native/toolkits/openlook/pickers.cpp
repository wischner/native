//
// Extends the native XView editable combo with a native Panel calendar
// or paired OLGX button arrows. Date text uses DD.MM.YYYY, and time
// text uses HH:MM[:SS]; shared policies accept or reject every
// proposal.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include "../../emulated_tree.h"
#include <xview/svrimage.h>
#include <xview/win_input.h>
#include <X11/keysym.h>
#include <charconv>
#include <array>
#include <cstdlib>
namespace
{
    using namespace native;
    using namespace linux::openlook;
    struct picker_state
    {
        panel &owner;
        combo_box field{{}, combo_box_style::editable};
        Frame popup = XV_NULL;
        Panel calendar = XV_NULL;
        Panel_item heading = XV_NULL, previous = XV_NULL, up = XV_NULL,
                   down = XV_NULL;
        Server_image up_image = XV_NULL, down_image = XV_NULL;
        date shown;
        struct day
        {
            picker_state *state = nullptr;
            unsigned value = 0;
            Panel_item item = XV_NULL;
        };
        std::array<day, 42> days;
        bool open = false;
        explicit picker_state(panel &value)
            : owner(value) {
            field.set_parent(&owner);
            field.create();
            field.show();
        }
        ~picker_state() {
            if (popup)
                xv_destroy_safe(popup);
            if (up)
                xv_destroy_safe(up);
            if (down)
                xv_destroy_safe(down);
            if (up_image)
                xv_destroy_safe(up_image);
            if (down_image)
                xv_destroy_safe(down_image);
        }
    };
    std::string date_text(date value) {
        auto two = [](unsigned value) {
            return (value < 10 ? "0" : "") + std::to_string(value);
        };
        return two(value.day) + "." + two(value.month) + "." +
               std::to_string(value.year);
    }
    bool number(std::string_view text, unsigned &value) {
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() &&
               result.ptr == text.data() + text.size();
    }
    void commit(picker_state &state) {
        const std::string text = state.field.get_text();
        const std::string_view value(text);
        unsigned first = 0, second = 0, third = 0;
        if (auto *owner = dynamic_cast<date_picker *>(&state.owner)) {
            if (value.size() == 10 && value[2] == '.' &&
                value[5] == '.' && number(value.substr(0, 2), first) &&
                number(value.substr(3, 2), second) &&
                number(value.substr(6, 4), third))
                owner->on_native_date({int(third), second, first});
            else
                owner->on_native_date(owner->get_date());
        } else if (auto *owner =
                       dynamic_cast<time_picker *>(&state.owner)) {
            third = owner->get_time().second;
            if ((value.size() == 5 || value.size() == 8) &&
                value[2] == ':' && number(value.substr(0, 2), first) &&
                number(value.substr(3, 2), second) &&
                (value.size() == 5 ||
                 (value[5] == ':' &&
                  number(value.substr(6, 2), third))))
                owner->on_native_time({first, second, third});
            else
                owner->on_native_time(owner->get_time());
        }
    }
    void close(picker_state &state) {
        state.open = false;
        if (state.popup)
            xv_set(state.popup, XV_SHOW, FALSE, nullptr);
    }
    void calendar(picker_state &state) {
        auto &owner = static_cast<date_picker &>(state.owner);
        const auto title = std::to_string(state.shown.year) + "." +
                           std::to_string(state.shown.month);
        xv_set(
            state.heading, PANEL_LABEL_STRING, title.c_str(), nullptr);
        const int offset =
            int(date{state.shown.year, state.shown.month, 1}.weekday());
        for (unsigned i = 0; i < state.days.size(); ++i) {
            auto &day = state.days[i];
            const int value = int(i) - offset + 1;
            const date proposal{state.shown.year,
                                state.shown.month,
                                unsigned(std::max(1, value))};
            day.value =
                value > 0 && proposal.valid() ? unsigned(value) : 0;
            const auto label =
                day.value ? std::to_string(day.value) : "";
            xv_set(day.item,
                   PANEL_LABEL_STRING,
                   label.c_str(),
                   PANEL_INACTIVE,
                   !day.value || !owner.is_date_enabled(proposal),
                   XV_SHOW,
                   day.value != 0,
                   nullptr);
        }
    }
    void month(Panel_item item, Event *) {
        auto *state = reinterpret_cast<picker_state *>(
            xv_get(item, PANEL_CLIENT_DATA));
        const int direction = item == state->previous ? -1 : 1;
        const int value =
            std::clamp(state->shown.year * 12 +
                           int(state->shown.month) - 1 + direction,
                       1601 * 12,
                       9999 * 12 + 11);
        state->shown = {value / 12, unsigned(value % 12 + 1), 1};
        calendar(*state);
        xv_set(item, PANEL_NOTIFY_STATUS, XV_ERROR, nullptr);
    }
    void pick(Panel_item item, Event *) {
        auto *day = reinterpret_cast<picker_state::day *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto &state = *day->state;
        auto &owner = static_cast<date_picker &>(state.owner);
        const date value{
            state.shown.year, state.shown.month, day->value};
        if (!owner.is_date_enabled(value))
            return;
        close(state);
        owner.on_native_date(value);
    }
    Notify_value calendar_event(Notify_client client,
                                Notify_event input,
                                Notify_arg arg,
                                Notify_event_type type) {
        auto *event = reinterpret_cast<Event *>(input);
        auto *state = reinterpret_cast<picker_state *>(
            xv_get(client, WIN_CLIENT_DATA));
        auto *raw = event_xevent(event);
        if (state && raw && raw->type == KeyPress &&
            XLookupKeysym(&raw->xkey, 0) == XK_Escape) {
            close(*state);
            return NOTIFY_DONE;
        }
        return notify_next_event_func(client, input, arg, type);
    }
    void popup(picker_state &state) {
        if (state.open) {
            close(state);
            return;
        }
        const int cell =
            std::max(40, int(state.field.get_dimensions().w) / 7);
        const int row = 26;
        if (!state.popup) {
            auto *root =
                dynamic_cast<app_wnd *>(detail::root_of(&state.owner));
            const auto frame = window_state(root)->frame;
            state.popup = xv_create(frame,
                                    FRAME_CMD,
                                    FRAME_SHOW_HEADER,
                                    FALSE,
                                    FRAME_CMD_PUSHPIN_IN,
                                    FALSE,
                                    XV_SHOW,
                                    FALSE,
                                    nullptr);
            state.calendar = xv_get(state.popup, FRAME_CMD_PANEL);
            xv_set(state.calendar,
                   WIN_CLIENT_DATA,
                   &state,
                   PANEL_ACCEPT_KEYSTROKE,
                   TRUE,
                   nullptr);
            notify_interpose_event_func(
                state.calendar,
                reinterpret_cast<Notify_func>(calendar_event),
                NOTIFY_SAFE);
            for (int direction : {-1, 1}) {
                auto item = xv_create(state.calendar,
                                      PANEL_BUTTON,
                                      PANEL_LABEL_STRING,
                                      direction < 0 ? "<" : ">",
                                      PANEL_CLIENT_DATA,
                                      &state,
                                      PANEL_NOTIFY_PROC,
                                      month,
                                      XV_X,
                                      direction < 0 ? 2 : cell * 6,
                                      XV_Y,
                                      2,
                                      nullptr);
                if (direction < 0)
                    state.previous = item;
            }
            state.heading = xv_create(state.calendar,
                                      PANEL_MESSAGE,
                                      PANEL_LABEL_STRING,
                                      "",
                                      XV_X,
                                      cell * 2,
                                      XV_Y,
                                      6,
                                      nullptr);
            const char *names[] = {
                "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
            for (int i = 0; i < 7; ++i)
                xv_create(state.calendar,
                          PANEL_MESSAGE,
                          PANEL_LABEL_STRING,
                          names[i],
                          XV_X,
                          i * cell + 8,
                          XV_Y,
                          row + 2,
                          nullptr);
            for (unsigned i = 0; i < state.days.size(); ++i) {
                auto &day = state.days[i];
                day.state = &state;
                day.item = xv_create(state.calendar,
                                     PANEL_BUTTON,
                                     PANEL_LABEL_STRING,
                                     "00",
                                     PANEL_CLIENT_DATA,
                                     &day,
                                     PANEL_NOTIFY_PROC,
                                     pick,
                                     XV_X,
                                     int(i % 7) * cell + 2,
                                     XV_Y,
                                     int(i / 7 + 2) * row,
                                     nullptr);
                fit_item_width(day.item, cell - 4);
            }
            xv_set(state.calendar,
                   XV_WIDTH,
                   cell * 7 + 2,
                   XV_HEIGHT,
                   row * 8 + 2,
                   nullptr);
            xv_set(state.popup,
                   XV_WIDTH,
                   cell * 7 + 2,
                   XV_HEIGHT,
                   row * 8 + 2,
                   nullptr);
        }
        state.shown =
            static_cast<date_picker &>(state.owner).get_date();
        calendar(state);
        const auto *binding =
            combo_box_bindings.object_from_handle(&state.field);
        const auto panel = xv_get(binding->text, XV_OWNER);
        const auto *bounds =
            reinterpret_cast<Rect *>(xv_get(binding->text, XV_RECT));
        Window child = None;
        int x = 0, y = 0;
        XTranslateCoordinates(cached_display,
                              xv_get(panel, XV_XID),
                              DefaultRootWindow(cached_display),
                              bounds->r_left,
                              bounds->r_top + bounds->r_height,
                              &x,
                              &y,
                              &child);
        xv_set(state.popup, XV_X, x, XV_Y, y, XV_SHOW, TRUE, nullptr);
        state.open = true;
    }
    void step(Panel_item item, Event *) {
        auto *state = reinterpret_cast<picker_state *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto &owner = static_cast<time_picker &>(state->owner);
        const auto before = owner.get_time();
        const int direction = item == state->up ? 1 : -1;
        const int seconds = std::clamp(
            int(before.hour * 3600 + before.minute * 60 +
                before.second) +
                direction * int(owner.get_minute_step()) * 60,
            0,
            86399);
        owner.on_native_time({unsigned(seconds / 3600),
                              unsigned(seconds / 60 % 60),
                              unsigned(seconds % 60)});
    }
    picker_state &create(panel &owner) {
        auto value = std::make_unique<picker_state>(owner);
        combo_box_bindings.object_from_handle(&value->field)->commit =
            [state = value.get()] {
                commit(*state);
            };
        auto &result = *value;
        detail::assign_peer_state(owner, value.release());
        return result;
    }
    void refresh(picker_state &state, const std::string &text) {
        state.field.set_bounds(state.owner.get_client_bounds());
        state.field.set_text(text);
        auto *binding =
            combo_box_bindings.object_from_handle(&state.field);
        const Rect text_bounds =
            *reinterpret_cast<Rect *>(xv_get(binding->text, XV_RECT));
        const int height = state.owner.get_client_bounds().h();
        const int text_y =
            std::max(0, (height - text_bounds.r_height) / 2);
        xv_set(binding->text, XV_Y, text_y, nullptr);
        xv_set(binding->choice, XV_Y, text_y, nullptr);
        if (state.up) {
            xv_set(binding->choice, XV_SHOW, FALSE, nullptr);
            const int x = text_bounds.r_left + text_bounds.r_width;
            const int up_height = xv_get(state.up, XV_HEIGHT);
            const int down_height = xv_get(state.down, XV_HEIGHT);
            const int top =
                std::max(0, (height - up_height - down_height) / 2);
            xv_set(state.up, XV_X, x, XV_Y, top, nullptr);
            xv_set(state.down, XV_X, x, XV_Y, top + up_height, nullptr);
        }
        if (state.open)
            calendar(state);
    }
} // namespace
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        auto &state = create(owner);
        linux::openlook::combo_box_bindings
            .object_from_handle(&state.field)
            ->open_popup = [&state](Event *) {
            popup(state);
        };
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        auto &state = create(owner);
        auto *binding =
            linux::openlook::combo_box_bindings.object_from_handle(
                &state.field);
        const auto panel = xv_get(binding->text, XV_OWNER);
        static const unsigned char up[] = {0x38, 0, 0x7c, 0};
        static const unsigned char down[] = {0x7c, 0, 0x38, 0};
        state.up_image = xv_create(XV_NULL,
                                   SERVER_IMAGE,
                                   XV_WIDTH,
                                   9,
                                   XV_HEIGHT,
                                   2,
                                   SERVER_IMAGE_DEPTH,
                                   1,
                                   SERVER_IMAGE_X_BITS,
                                   up,
                                   nullptr);
        state.down_image = xv_create(XV_NULL,
                                     SERVER_IMAGE,
                                     XV_WIDTH,
                                     9,
                                     XV_HEIGHT,
                                     2,
                                     SERVER_IMAGE_DEPTH,
                                     1,
                                     SERVER_IMAGE_X_BITS,
                                     down,
                                     nullptr);
        state.up = xv_create(panel,
                             PANEL_BUTTON,
                             PANEL_LABEL_STRING,
                             "",
                             PANEL_CLIENT_DATA,
                             &state,
                             PANEL_NOTIFY_PROC,
                             step,
                             nullptr);
        state.down = xv_create(panel,
                               PANEL_BUTTON,
                               PANEL_LABEL_STRING,
                               "",
                               PANEL_CLIENT_DATA,
                               &state,
                               PANEL_NOTIFY_PROC,
                               step,
                               nullptr);
        const auto image_button = [](Panel_item item,
                                     Server_image image) {
#ifdef __linux__
            // XView's Linux panel_make_image frees the previous label
            // only on its string path. The server-image path overwrites
            // it.
            auto *previous = reinterpret_cast<char *>(
                xv_get(item, PANEL_LABEL_STRING));
#endif
            xv_set(item, PANEL_LABEL_IMAGE, image, nullptr);
#ifdef __linux__
            std::free(previous);
#endif
        };
        image_button(state.up, state.up_image);
        image_button(state.down, state.down_image);
        return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        if (auto *state = peer_state<picker_state>(owner))
            refresh(*state, date_text(owner.get_date()));
    }
    void refresh_time_picker_peer(time_picker &owner) {
        if (auto *state = peer_state<picker_state>(owner))
            refresh(*state,
                    owner.get_time().to_string(
                        owner.get_seconds_visible()));
    }
} // namespace native::detail
