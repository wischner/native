//
// Exercises SDL event routing for editable fields, table modifiers,
// completion popups and controls inside tabs, plus picker/calendar
// pixels and bounded repeated-frame text uploads.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/sdl2/globals.h"
#include "../lib/native/code_render.h"
#include "../lib/native/control_render_access.h"
#include <iostream>
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace
{
    using namespace native;
    using namespace linux::sdl2;
    class rows final : public table_model
    {
    public:
        mutable int reads = 0;
        std::size_t row_count() const override {
            return 1000000;
        }
        table_row_id row_id(std::size_t row) const override {
            return row + 1;
        }
        table_cell cell(std::size_t row,
                        table_column_id) const override {
            ++reads;
            return {"Record " + std::to_string(row + 1), nullptr};
        }
    };
    void expect(bool valid, const char *message) {
        if (!valid)
            throw std::runtime_error(message);
    }
    class controls final : public app_wnd
    {
        rows model;
        table_view table{10, 10, 390, 190};
        code_edit code{"int value;", 10, 225, 390, 200};
        combo_box combo{{"10 mm", "25 mm"},
                        combo_box_style::editable,
                        430,
                        10,
                        240,
                        30};
        panel page;
        list choices{{"One", "Two", "Three"}, 10, 10, 180, 90};
        tab_view tabs{430, 65, 300, 170};
        date_picker dates{date{2026, 12, 24}, rect(430, 260, 240, 30)};
        time_picker times{time_of_day{9, 30, 0},
                          rect(430, 310, 240, 30)};
        text_edit notes{"Read-only help uses the standard text field "
                        "border and paper.",
                        text_edit_mode::multi_line,
                        10,
                        10,
                        220,
                        100};
        panel settings;
        settings_sidebar sidebar{430, 365, 430, 310};
        std::string accepted;
        int stage = 0;

    public:
        bool failed = false;
        controls()
            : app_wnd("SDL control regression", 20, 20, 900, 700) {
            table.set_model(&model)
                .set_data_mode(table_data_mode::virtualized)
                .set_columns({table_column{1, "Value", 280}})
                .set_header_visible(false)
                .set_row_height(24)
                .set_selection_mode(table_selection_mode::multiple);
            tabs.add_item("Page", page);
            page.on_wnd_create.connect([this] {
                attach(choices, page);
                return true;
            });
            dates.set_disabled_dates({date{2026, 12, 25}});
            notes.set_read_only(true);
            settings.on_wnd_create.connect([this] {
                attach(notes, settings);
                return true;
            });
            sidebar.set_sidebar_width(180).set_search_visible(true);
            sidebar << settings_page(
                1, "General", settings, nullptr, "Settings");
            code.on_complete.connect([this](completion_item item) {
                accepted = item.label;
                return true;
            });
            on_wnd_create.connect([this] {
                for (wnd *control : std::array<wnd *, 7>{&table,
                                                         &code,
                                                         &combo,
                                                         &tabs,
                                                         &dates,
                                                         &times,
                                                         &sidebar})
                    attach(*control, *this);
                later();
                return true;
            });
        }

    private:
        static void attach(wnd &control, wnd &parent) {
            control.set_parent(&parent);
            control.create();
            control.show();
        }
        void later() {
            SDL_AddTimer(
                25,
                +[](Uint32, void *data) -> Uint32 {
                    auto *self = static_cast<controls *>(data);
                    app::post([self] {
                        self->advance();
                    });
                    return 0;
                },
                this);
        }
        Uint32 id() {
            return SDL_GetWindowID(
                wnd_bindings.handle_from_object(this));
        }
        void click(point position) {
            for (auto type : {SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP}) {
                SDL_Event event{};
                event.type = type;
                event.button.windowID = id();
                event.button.button = SDL_BUTTON_LEFT;
                event.button.clicks = 1;
                event.button.x = position.x;
                event.button.y = position.y + content_origin_y(this);
                SDL_PushEvent(&event);
            }
        }
        void click(wnd &control, int x, int y) {
            const auto origin = origin_in_root(control);
            click(point(origin.x + x, origin.y + y));
        }
        void key(SDL_Keycode key, SDL_Keymod modifiers = KMOD_NONE) {
            for (auto type : {SDL_KEYDOWN, SDL_KEYUP}) {
                SDL_Event event{};
                event.type = type;
                event.key.windowID = id();
                event.key.keysym.sym = key;
                event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
                event.key.keysym.mod = modifiers;
                SDL_PushEvent(&event);
            }
        }
        void text(const char *value) {
            SDL_Event event{};
            event.type = SDL_TEXTINPUT;
            event.text.windowID = id();
            SDL_strlcpy(
                event.text.text, value, sizeof(event.text.text));
            SDL_PushEvent(&event);
        }
        combo_box &field(panel &owner) {
            for (auto *value : combo_boxes)
                if (value->get_parent() == &owner)
                    return *value;
            throw std::runtime_error(
                "picker has one editable combo field");
        }
        void render() {
            invalidate();
            render_window_if_needed(this);
        }
        void snapshot(const std::string &path) {
            render();
            auto *cache = wnd_gpx_bindings.object_from_handle(this);
            img capture(900, 700);
            SDL_RenderReadPixels(cache->renderer,
                                 nullptr,
                                 SDL_PIXELFORMAT_RGBA32,
                                 capture.pixels(),
                                 900 * 4);
            capture.save(path);
        }
        void pixels() {
            img actual(220, 65), primitive(220, 65);
            list value({"First", "Second"}, 0, 0, 220, 65);
            value.set_selected_index(1);
            theme::state state;
            auto &a = actual.get_gpx();
            auto &b = primitive.get_gpx();
            auto ta = theme::create(a), tb = theme::create(b);
            detail::control_render_access::draw(
                value, a, *ta, rect(0, 0, 220, 65), state);
            tb->draw_list(
                rect(0, 0, 220, 65), value.get_items(), 1, state);
            for (int i = 0; i < 220 * 65; ++i) {
                if (actual.pixels()[i] != primitive.pixels()[i]) {
                    const auto a = actual.pixels()[i],
                               b = primitive.pixels()[i];
                    std::cerr << "List mismatch " << i % 220 << ','
                              << i / 220 << " actual " << int(a.r)
                              << ',' << int(a.g) << ',' << int(a.b)
                              << " primitive " << int(b.r) << ','
                              << int(b.g) << ',' << int(b.b) << '\n';
                    actual.save("sdl-list-actual.png");
                    primitive.save("sdl-list-primitive.png");
                }
                expect(actual.pixels()[i] == primitive.pixels()[i],
                       "list primitive matches actual control pixels, "
                       "including side margins");
            }
            render();
            auto *cache = wnd_gpx_bindings.object_from_handle(this);
            const auto uploads = cache->text_uploads;
#ifdef HAVE_SDL2_TTF
            expect(uploads > 0,
                   "table uses the renderer-owned text cache");
#endif
            const int reads = model.reads;
            render();
            expect(model.reads - reads < 100,
                   "million-row table only requests visible cells");
            expect(cache->text_uploads == uploads,
                   "unchanged table frames reuse text textures");
            expect(cache->text_bytes <= 4 * 1024 * 1024 &&
                       cache->text_textures.size() <= 256,
                   "text textures stay within their per-window budget");
            auto &g = get_gpx();
            auto appearance = theme::create(g);
            g.set_clip(rect(0, 0, 900, 700));
            appearance->draw_menu_bar(rect(780, 10, 50, 180));
            rgba edge, center;
            SDL_Rect left{780, 60 + content_origin_y(this), 1, 1};
            SDL_Rect mid{800, 60 + content_origin_y(this), 1, 1};
            SDL_RenderReadPixels(cache->renderer,
                                 &left,
                                 SDL_PIXELFORMAT_RGBA32,
                                 &edge,
                                 4);
            SDL_RenderReadPixels(cache->renderer,
                                 &mid,
                                 SDL_PIXELFORMAT_RGBA32,
                                 &center,
                                 4);
            expect(edge != center,
                   "vertical toolbar strip has a vertical edge rule");
        }
        void advance() {
            try {
                switch (stage++) {
                case 0:
                    pixels();
                    click(table, 30, 12);
                    break;
                case 1:
                    expect(table.get_selected_rows().size() == 1,
                           "plain table selection");
                    SDL_SetModState(KMOD_CTRL);
                    click(table, 30, 36);
                    break;
                case 2:
                    expect(table.get_selected_rows().size() == 2,
                           "Ctrl adds a table selection");
                    SDL_SetModState(KMOD_SHIFT);
                    click(table, 30, 84);
                    break;
                case 3:
                    expect(table.get_selected_rows().size() >= 3,
                           "Shift extends a table range");
                    SDL_SetModState(KMOD_NONE);
                    click(combo, 30, 12);
                    key(SDLK_a, KMOD_CTRL);
                    text("50 mm");
                    break;
                case 4:
                    expect(combo.get_text() == "50 mm" &&
                               combo.get_selected_index() == -1,
                           "combo text can be selected and replaced");
                    key(SDLK_RETURN);
                    click(combo, 230, 12);
                    break;
                case 5: {
                    auto *peer =
                        combo_box_bindings.object_from_handle(&combo);
                    expect(peer->open && combo.get_text() == "50 mm",
                           "opening the combo preserves typed text");
                    click(
                        combo, 30, int(combo.get_dimensions().h) + 30);
                    break;
                }
                case 6:
                    expect(combo.get_selected_index() == 1 &&
                               combo.get_text() == "25 mm",
                           "popup choice synchronizes editable text");
                    expect(
                        combo_box_bindings.object_from_handle(&combo)
                                ->editor->get_text() == "25 mm",
                        "embedded combo editor follows popup choice");
                    click(choices, 25, 30);
                    break;
                case 7:
                    expect(choices.get_selected_index() == 1,
                           "list inside a tab receives mouse clicks");
                    code.show_completion({{"First", "first", ""},
                                          {"Second", "second", ""},
                                          {"Third", "third", ""}});
                    key(SDLK_DOWN);
                    key(SDLK_RETURN);
                    break;
                case 8:
                    expect(accepted == "Second",
                           "completion Down and Enter work after "
                           "sibling focus");
                    accepted.clear();
                    code.show_completion({{"First", "first", ""},
                                          {"Second", "second", ""},
                                          {"Third", "third", ""}});
                    render();
                    {
                        const auto *popup = detail::peer_state<
                            detail::code_completion_geometry>(code);
                        expect(popup,
                               "completion has painted hit geometry");
                        click(code,
                              popup->bounds.x1() + 20,
                              popup->bounds.y1() +
                                  popup->row_height * 2 + 2);
                    }
                    break;
                case 9:
                    expect(accepted == "Third",
                           "mouse selects the painted completion row");
                    click(field(dates), 20, 12);
                    key(SDLK_a, KMOD_CTRL);
                    text("2026-12-25");
                    key(SDLK_RETURN);
                    break;
                case 10:
                    expect(dates.get_date() == date{2026, 12, 24},
                           "typed holiday is rejected");
                    expect(field(dates).get_text() == "2026-12-24",
                           "rejected date restores field text");
                    click(field(dates), 230, 12);
                    break;
                case 11: {
                    auto &date_field = field(dates);
                    expect(combo_box_bindings
                               .object_from_handle(&date_field)
                               ->open,
                           "date arrow opens calendar");
                    if (const char *path =
                            std::getenv("NATIVE_SDL_CONTROL_SNAPSHOT"))
                        snapshot(std::string(path) + ".calendar.png");
                    const auto bounds = root_bounds(date_field);
                    const int row = theme::create(get_gpx())
                                        ->get_list_item_height();
                    // December 2026 starts Tuesday; Friday the 25th is
                    // disabled.
                    click(point(bounds.x1() + int(bounds.w()) * 5 / 7 +
                                    10,
                                bounds.y2() + 1 + row * 5 + row / 2));
                    break;
                }
                case 12:
                    expect(dates.get_date() == date{2026, 12, 24} &&
                               combo_box_bindings
                                   .object_from_handle(&field(dates))
                                   ->open,
                           "disabled calendar day cannot commit");
                    click(field(dates), 230, 12);
                    break;
                case 13:
                    expect(!combo_box_bindings
                                .object_from_handle(&field(dates))
                                ->open,
                           "second date arrow click closes calendar");
                    click(field(times), 20, 12);
                    key(SDLK_a, KMOD_CTRL);
                    text("12:45");
                    key(SDLK_RETURN);
                    break;
                case 14:
                    expect(times.get_time() == time_of_day{12, 45, 0},
                           "joined time editor accepts HH:MM");
                    times.set_minute_step(15);
                    click(field(times), 230, 4);
                    break;
                case 15:
                    expect(times.get_time() == time_of_day{13, 0, 0},
                           "time arrow respects the selected minute "
                           "field and step");
                    expect(field(dates).get_dimensions().h ==
                                   combo.get_dimensions().h &&
                               field(times).get_dimensions().h ==
                                   combo.get_dimensions().h,
                           "date and time fields match standard combo "
                           "height");
                    for (auto *tree : tree_views)
                        if (tree->get_parent() == &sidebar) {
                            const auto bounds = tree->get_bounds();
                            expect(bounds.x1() == 6 &&
                                       bounds.y2() ==
                                           int(sidebar.get_dimensions()
                                                   .h) -
                                               6,
                                   "settings navigation has equal "
                                   "outer margins");
                            expect(tree->get_border_visible(),
                                   "settings navigation is framed like "
                                   "search");
                        }
                    if (const char *path =
                            std::getenv("NATIVE_SDL_CONTROL_SNAPSHOT"))
                        snapshot(path);
                    destroy();
                    return;
                }
                later();
            } catch (const std::exception &error) {
                std::cerr << "FAILED stage " << stage - 1 << ": "
                          << error.what() << '\n';
                failed = true;
                destroy();
            }
        }
    };
} // namespace
int program(int, char **) {
    controls window;
    const auto result = app::run(window);
    return result ? result : window.failed ? 1 : 0;
}
