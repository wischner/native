//
// Composes Athena date/time fields with the existing editable combo
// editor. The calendar popup uses native Toggle/Command/Label widgets
// and disabled dates; time stepping replaces the combo arrow with two
// native Command halves.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "scale.h"
#include "../../picker_peer.h"
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <charconv>
#include <chrono>

namespace
{
    using namespace native;
    using namespace linux::x11;
    bool number(std::string_view text, unsigned &value) {
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() &&
               result.ptr == text.data() + text.size();
    }
    struct picker_state
    {
        panel &owner;
        combo_box field{{}, combo_box_style::editable};
        Widget popup = nullptr, grid = nullptr, heading = nullptr;
        Widget down = nullptr;
        date shown;
        bool updating = false, open = false;
        struct day_button
        {
            picker_state *peer;
            unsigned day;
            Widget widget;
        };
        std::vector<std::unique_ptr<day_button>> days;
        explicit picker_state(panel &value)
            : owner(value) {
            field.set_parent(&owner);
            field.create();
            field.show();
        }
    };
    void calendar(picker_state &state);
    void commit(picker_state &state) {
        if (state.updating)
            return;
        const std::string_view text = state.field.get_text();
        unsigned first = 0, second = 0, third = 0;
        if (auto *owner = dynamic_cast<date_picker *>(&state.owner)) {
            if (text.size() == 10 && text[4] == '-' && text[7] == '-' &&
                number(text.substr(0, 4), first) &&
                number(text.substr(5, 2), second) &&
                number(text.substr(8, 2), third))
                owner->on_native_date({int(first), second, third});
            else
                owner->on_native_date(owner->get_date());
        } else if (auto *owner =
                       dynamic_cast<time_picker *>(&state.owner)) {
            third = owner->get_time().second;
            if ((text.size() == 5 || text.size() == 8) &&
                text[2] == ':' && number(text.substr(0, 2), first) &&
                number(text.substr(3, 2), second) &&
                (text.size() == 5 ||
                 (text[5] == ':' && number(text.substr(6, 2), third))))
                owner->on_native_time({first, second, third});
            else
                owner->on_native_time(owner->get_time());
        }
    }
    void key(Widget, XtPointer data, XEvent *event, Boolean *) {
        if (event->type == KeyRelease &&
            (XLookupKeysym(&event->xkey, 0) == XK_Return ||
             XLookupKeysym(&event->xkey, 0) == XK_KP_Enter))
            commit(*static_cast<picker_state *>(data));
        if (event->type == FocusOut)
            commit(*static_cast<picker_state *>(data));
    }
    void dismiss(picker_state &state) {
        if (!state.open)
            return;
        state.open = false;
        XtPopdown(state.popup);
    }
    void
    outside(Widget, XtPointer data, XEvent *event, Boolean *dispatch) {
        auto &state = *static_cast<picker_state *>(data);
        if (event->type != ButtonPress)
            return;
        Dimension width = 0, height = 0;
        XtVaGetValues(
            state.popup, XtNwidth, &width, XtNheight, &height, nullptr);
        Position x = 0, y = 0;
        XtTranslateCoords(state.popup, 0, 0, &x, &y);
        if (event->xbutton.x_root < x || event->xbutton.y_root < y ||
            event->xbutton.x_root >= x + width ||
            event->xbutton.y_root >= y + height) {
            dismiss(state);
            *dispatch = False;
        }
    }
    void month(picker_state &state, int delta) {
        int value =
            state.shown.year * 12 + int(state.shown.month) - 1 + delta;
        value = std::clamp(value, 1601 * 12, 9999 * 12 + 11);
        state.shown = {value / 12, unsigned(value % 12 + 1), 1};
        calendar(state);
    }
    void calendar(picker_state &state) {
        auto &owner = static_cast<date_picker &>(state.owner);
        std::string title = std::to_string(state.shown.year) + "-" +
                            (state.shown.month < 10 ? "0" : "") +
                            std::to_string(state.shown.month);
        XtVaSetValues(state.heading, XtNlabel, title.c_str(), nullptr);
        const auto first = date{state.shown.year, state.shown.month, 1};
        const int offset = int(first.weekday());
        const auto height = to_device(
            theme::create(owner.get_gpx())->get_text_edit_height());
        Dimension width = 0;
        XtVaGetValues(state.grid, XtNwidth, &width, nullptr);
        const int cell = std::max(1, int(width) / 7);
        for (unsigned i = 0; i < state.days.size(); ++i) {
            auto &button = *state.days[i];
            const int day = int(i) - offset + 1;
            const date value{state.shown.year,
                             state.shown.month,
                             unsigned(std::max(1, day))};
            button.day = day > 0 && value.valid() ? unsigned(day) : 0;
            const auto text =
                button.day ? std::to_string(button.day) : "";
            XtVaSetValues(button.widget,
                          XtNlabel,
                          text.c_str(),
                          XtNstate,
                          button.day && value == owner.get_date(),
                          XtNsensitive,
                          button.day && owner.is_date_enabled(value),
                          XtNhorizDistance,
                          (i % 7) * cell,
                          XtNvertDistance,
                          (i / 7 + 2) * height,
                          XtNwidth,
                          cell - 2,
                          XtNheight,
                          height - 2,
                          nullptr);
        }
    }
    void popup(picker_state &state) {
        if (state.open) {
            dismiss(state);
            return;
        }
        auto &owner = static_cast<date_picker &>(state.owner);
        state.shown = owner.get_date();
        auto *binding =
            combo_box_bindings.object_from_handle(&state.field);
        if (!state.popup) {
            state.popup = XtVaCreatePopupShell("dateCalendar",
                                               overrideShellWidgetClass,
                                               binding->root,
                                               XtNborderWidth,
                                               1,
                                               nullptr);
            state.grid = XtVaCreateManagedWidget("calendar",
                                                 layout_host_class(),
                                                 state.popup,
                                                 XtNborderWidth,
                                                 0,
                                                 XtNdefaultDistance,
                                                 0,
                                                 nullptr);
            const auto height = to_device(
                theme::create(owner.get_gpx())->get_text_edit_height());
            auto *previous = XtVaCreateManagedWidget("previousMonth",
                                                     commandWidgetClass,
                                                     state.grid,
                                                     XtNlabel,
                                                     "<",
                                                     XtNwidth,
                                                     height - 2,
                                                     XtNheight,
                                                     height - 2,
                                                     nullptr);
            state.heading = XtVaCreateManagedWidget("month",
                                                    labelWidgetClass,
                                                    state.grid,
                                                    XtNborderWidth,
                                                    0,
                                                    XtNresize,
                                                    False,
                                                    nullptr);
            auto *next = XtVaCreateManagedWidget("nextMonth",
                                                 commandWidgetClass,
                                                 state.grid,
                                                 XtNlabel,
                                                 ">",
                                                 XtNwidth,
                                                 height - 2,
                                                 XtNheight,
                                                 height - 2,
                                                 nullptr);
            XtAddCallback(
                previous,
                XtNcallback,
                +[](Widget, XtPointer data, XtPointer) {
                    month(*static_cast<picker_state *>(data), -1);
                },
                &state);
            XtAddCallback(
                next,
                XtNcallback,
                +[](Widget, XtPointer data, XtPointer) {
                    month(*static_cast<picker_state *>(data), 1);
                },
                &state);
            const char *names[] = {
                "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
            for (unsigned i = 0; i < 7; ++i)
                XtVaCreateManagedWidget(names[i],
                                        labelWidgetClass,
                                        state.grid,
                                        XtNlabel,
                                        names[i],
                                        XtNborderWidth,
                                        0,
                                        nullptr);
            for (unsigned i = 0; i < 42; ++i) {
                auto value =
                    std::make_unique<picker_state::day_button>();
                value->peer = &state;
                value->widget =
                    XtVaCreateManagedWidget("day",
                                            toggleWidgetClass,
                                            state.grid,
                                            XtNlabel,
                                            "",
                                            XtNresize,
                                            False,
                                            nullptr);
                XtAddCallback(
                    value->widget,
                    XtNcallback,
                    +[](Widget, XtPointer data, XtPointer) {
                        auto &day =
                            *static_cast<picker_state::day_button *>(
                                data);
                        auto &state = *day.peer;
                        const auto value = date{state.shown.year,
                                                state.shown.month,
                                                day.day};
                        dismiss(state);
                        static_cast<date_picker &>(state.owner)
                            .on_native_date(value);
                    },
                    value.get());
                state.days.push_back(std::move(value));
            }
            XtAddEventHandler(
                state.popup, ButtonPressMask, False, outside, &state);
        }
        Dimension field_width = 0, field_height = 0;
        XtVaGetValues(binding->root,
                      XtNwidth,
                      &field_width,
                      XtNheight,
                      &field_height,
                      nullptr);
        const int height = to_device(
            theme::create(owner.get_gpx())->get_text_edit_height());
        const int width =
            std::max(int(field_width) - 2, to_device(224));
        const int cell = width / 7;
        XtVaSetValues(state.grid,
                      XtNwidth,
                      width,
                      XtNheight,
                      8 * height,
                      nullptr);
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(state.grid,
                      XtNchildren,
                      &children,
                      XtNnumChildren,
                      &count,
                      nullptr);
        XtVaSetValues(children[0],
                      XtNhorizDistance,
                      0,
                      XtNvertDistance,
                      0,
                      nullptr);
        XtVaSetValues(state.heading,
                      XtNhorizDistance,
                      height,
                      XtNvertDistance,
                      0,
                      XtNwidth,
                      width - 2 * height,
                      XtNheight,
                      height,
                      nullptr);
        XtVaSetValues(children[2],
                      XtNhorizDistance,
                      width - height,
                      XtNvertDistance,
                      0,
                      nullptr);
        for (int i = 0; i < 7; ++i)
            XtVaSetValues(children[i + 3],
                          XtNhorizDistance,
                          i * cell,
                          XtNvertDistance,
                          height,
                          XtNwidth,
                          cell,
                          XtNheight,
                          height,
                          nullptr);
        calendar(state);
        Position x = 0, y = 0;
        XtTranslateCoords(binding->root, 0, field_height, &x, &y);
        const int screen_width = WidthOfScreen(XtScreen(state.popup));
        const int screen_height = HeightOfScreen(XtScreen(state.popup));
        const int popup_height = 8 * height + 2;
        const int top = y + popup_height <= screen_height
                            ? y
                            : y - field_height - popup_height;
        XtVaSetValues(state.popup,
                      XtNx,
                      std::clamp<int>(
                          x, 0, std::max(0, screen_width - width - 2)),
                      XtNy,
                      std::max(0, top),
                      nullptr);
        XtRealizeWidget(state.popup);
        const auto arrow =
            XCreateFontCursor(cached_display, XC_left_ptr);
        XDefineCursor(cached_display, XtWindow(state.popup), arrow);
        XFreeCursor(cached_display, arrow);
        XtPopupSpringLoaded(state.popup);
        XGrabPointer(cached_display,
                     XtWindow(state.popup),
                     True,
                     ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync,
                     GrabModeAsync,
                     None,
                     None,
                     CurrentTime);
        state.open = true;
    }
    picker_state &create(panel &owner) {
        auto value = std::make_unique<picker_state>(owner);
        auto *binding =
            combo_box_bindings.object_from_handle(&value->field);
        XtAddEventHandler(binding->text,
                          KeyReleaseMask | FocusChangeMask,
                          False,
                          key,
                          value.get());
        auto *result = value.get();
        detail::assign_peer_state(owner, value.release());
        return *result;
    }
    void step(picker_state &state, int direction) {
        commit(state);
        auto &owner = static_cast<time_picker &>(state.owner);
        auto *binding =
            combo_box_bindings.object_from_handle(&state.field);
        XawTextPosition begin = 0, end = 0;
        XawTextGetSelectionPos(binding->text, &begin, &end);
        const auto cursor =
            begin < end ? begin
                        : XawTextGetInsertionPoint(binding->text);
        auto value = owner.get_time();
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
    void refresh(picker_state &state, const std::string &text) {
        state.updating = true;
        auto *binding =
            combo_box_bindings.object_from_handle(&state.field);
        const auto cursor = XawTextGetInsertionPoint(binding->text);
        XawTextPosition begin = 0, end = 0;
        XawTextGetSelectionPos(binding->text, &begin, &end);
        XFontStruct *font = nullptr;
        XtVaGetValues(binding->text, XtNfont, &font, nullptr);
        const int margin =
            font ? std::max(1, XTextWidth(font, "0", 1) / 2)
                 : to_device(3);
        XtVaSetValues(binding->text,
                      XtNleftMargin,
                      margin,
                      XtNrightMargin,
                      margin,
                      nullptr);
        auto bounds = state.owner.get_client_bounds();
        const int height = to_device(bounds.h());
        const int column = state.down ? height / 2 : height;
        if (font) {
            const int width =
                XTextWidth(font, text.c_str(), text.size()) +
                2 * margin + column + 2;
            // Round outward at fractional desktop scales.
            int logical = to_logical(width);
            if (to_device(logical) < width)
                ++logical;
            bounds.d.w = std::min<int>(bounds.w(), logical);
        }
        state.field.set_text(text).set_bounds(bounds);
        XawTextSetInsertionPoint(
            binding->text,
            std::min<XawTextPosition>(cursor, text.size()));
        if (begin < end)
            XawTextSetSelection(
                binding->text,
                std::min<XawTextPosition>(begin, text.size()),
                std::min<XawTextPosition>(end, text.size()));
        if (binding->layout)
            binding->layout();
        state.updating = false;
    }
} // namespace
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        auto &state = create(owner);
        auto *binding =
            linux::x11::combo_box_bindings.object_from_handle(
                &state.field);
        XtUninstallTranslations(binding->button);
        XtOverrideTranslations(
            binding->button,
            XtParseTranslationTable(
                "<EnterWindow>: highlight()\n<LeaveWindow>: "
                "reset()\n<Btn1Down>: set()\n<Btn1Up>: notify() "
                "unset()"));
        XtAddCallback(
            binding->button,
            XtNcallback,
            +[](Widget, XtPointer data, XtPointer) {
                popup(*static_cast<picker_state *>(data));
            },
            &state);
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        auto &state = create(owner);
        auto *binding =
            linux::x11::combo_box_bindings.object_from_handle(
                &state.field);
        auto *root = binding->root;
        static constexpr char up_bits[] = {0x10,
                                           0x00,
                                           0x38,
                                           0x00,
                                           0x7c,
                                           0x00,
                                           static_cast<char>(0xfe),
                                           0x00,
                                           static_cast<char>(0xff),
                                           0x01};
        binding->reverse_arrow = linux::x11::create_device_bitmap(
            linux::x11::cached_display,
            RootWindowOfScreen(XtScreen(root)),
            up_bits,
            9,
            5);
        XtDestroyWidget(binding->button);
        binding->button = XtVaCreateManagedWidget("timeUp",
                                                  commandWidgetClass,
                                                  root,
                                                  XtNlabel,
                                                  "",
                                                  XtNborderWidth,
                                                  1,
                                                  XtNresize,
                                                  False,
                                                  XtNfromHoriz,
                                                  binding->text,
                                                  XtNhorizDistance,
                                                  -1,
                                                  nullptr);
        state.down = XtVaCreateManagedWidget("timeDown",
                                             commandWidgetClass,
                                             root,
                                             XtNlabel,
                                             "",
                                             XtNborderWidth,
                                             1,
                                             XtNresize,
                                             False,
                                             nullptr);
        XtVaSetValues(binding->button,
                      XtNlabel,
                      "",
                      XtNbitmap,
                      binding->reverse_arrow,
                      XtNinternalWidth,
                      0,
                      XtNinternalHeight,
                      0,
                      XtNjustify,
                      XtJustifyCenter,
                      nullptr);
        XtVaSetValues(state.down,
                      XtNlabel,
                      "",
                      XtNbitmap,
                      binding->arrow,
                      XtNinternalWidth,
                      0,
                      XtNinternalHeight,
                      0,
                      XtNjustify,
                      XtJustifyCenter,
                      nullptr);
        binding->layout = [&state] {
            auto *binding =
                linux::x11::combo_box_bindings.object_from_handle(
                    &state.field);
            Dimension width = 0, height = 0;
            XtVaGetValues(binding->root,
                          XtNwidth,
                          &width,
                          XtNheight,
                          &height,
                          nullptr);
            const int half = std::max(1, int(height) / 2);
            const int button = std::min(int(width), half);
            XtVaSetValues(binding->text,
                          XtNwidth,
                          std::max(1, int(width) - button - 1),
                          nullptr);
            XtVaSetValues(binding->button,
                          XtNfromHoriz,
                          nullptr,
                          XtNhorizDistance,
                          int(width) - button,
                          XtNwidth,
                          std::max(1, button - 2),
                          XtNheight,
                          std::max(1, button - 2),
                          nullptr);
            XtVaSetValues(state.down,
                          XtNhorizDistance,
                          int(width) - button,
                          XtNvertDistance,
                          half,
                          XtNwidth,
                          std::max(1, button - 2),
                          XtNheight,
                          std::max(1, button - 2),
                          nullptr);
        };
        XtAddCallback(
            binding->button,
            XtNcallback,
            +[](Widget, XtPointer data, XtPointer) {
                step(*static_cast<picker_state *>(data), 1);
            },
            &state);
        XtAddCallback(
            state.down,
            XtNcallback,
            +[](Widget, XtPointer data, XtPointer) {
                step(*static_cast<picker_state *>(data), -1);
            },
            &state);
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
