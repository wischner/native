//
// Implements the SDL2 modal file-browser shell, responsive geometry, control
// connections, and complete pointer, keyboard, window, and paint dispatch.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_browser.h"

#include <algorithm>
#include <utility>

#include <native/font.h>

#include "globals.h"

namespace linux::sdl2
{
    file_browser::file_browser(
        native::file_dialog &source,
        bool save,
        bool directory,
        std::string suggested_name,
        std::string default_extension,
        bool confirm_overwrite)
        : _source(source)
        , _save(save)
        , _directory_mode(directory)
        , _default_extension(std::move(default_extension))
        , _confirm_overwrite(confirm_overwrite)
        , _window(*source.get_owner(), source.get_title(),
                  0, 0, 760, 500)
        , _back(file_browser_button_icon::back,
                native::rect(14, 14, 30, 30))
        , _forward(file_browser_button_icon::forward,
                   native::rect(48, 14, 30, 30))
        , _up(file_browser_button_icon::up,
              native::rect(82, 14, 30, 30))
        , _breadcrumbs(native::rect(120, 14, 626, 30))
        , _location({}, native::text_edit_mode::single_line,
                    native::rect(0, 0, 0, 0))
        , _places(14, 68, 164, 346)
        , _entries(192, 68, 554, 346)
        , _name(std::move(suggested_name),
                native::text_edit_mode::single_line,
                native::rect(82, 452, 452, 30))
        , _accept(directory ? "Select Folder" : save ? "Save" : "Open",
                  544, 452, 96, 30)
        , _cancel("Cancel", 650, 452, 96, 30) {
        native::table_column place_column;
        place_column.id = 1;
        place_column.title = "Places";
        place_column.width = 148;
        place_column.resizable = false;
        place_column.reorderable = false;
        _places
            .set_model(&_place_store)
            .set_columns({place_column})
            .set_header_visible(false)
            .set_columns_reorderable(false)
            .set_columns_resizable(false)
            .set_fill_last_column(true)
            .set_alternating_rows(false)
            .set_grid_lines(native::table_grid_lines::none)
            .set_row_height(30)
            .set_icon_size(native::size(20, 20))
            .set_vertical_scrollbar_policy(
                native::scrollbar_policy::automatic)
            .set_horizontal_scrollbar_policy(
                native::scrollbar_policy::never);

        native::table_column name_column;
        name_column.id = 1;
        name_column.title = "Name";
        name_column.width = 330;
        native::table_column type_column;
        type_column.id = 2;
        type_column.title = "Type";
        type_column.width = 120;
        type_column.allow_image = false;
        native::table_column size_column;
        size_column.id = 3;
        size_column.title = "Size";
        size_column.width = 76;
        size_column.alignment = native::table_alignment::end;
        size_column.allow_image = false;
        _entries
            .set_model(&_entry_store)
            .set_columns({name_column, type_column, size_column})
            .set_header_visible(true)
            .set_columns_reorderable(false)
            .set_fill_last_column(true)
            .set_alternating_rows(true)
            .set_grid_lines(native::table_grid_lines::none)
            .set_row_height(28)
            .set_icon_size(native::size(20, 20))
            .set_vertical_scrollbar_policy(
                native::scrollbar_policy::automatic)
            .set_horizontal_scrollbar_policy(
                native::scrollbar_policy::never)
            .set_type_search_enabled(true);
        build_places();
        choose_initial_directory();
        connect_events();
        layout(_window.get_dimensions());
    }

    bool file_browser::run(std::vector<std::filesystem::path> &paths) {
        _window.center_to_parent();
        _window.create();
        for (native::wnd *control : controls()) {
            control->set_parent(&_window);
            control->create();
            control->show();
        }
        _window.show();
        run_loop();
        paths = _paths;
        return !_paths.empty();
    }

    std::vector<native::wnd *> file_browser::controls() {
        std::vector<native::wnd *> result{
            &_back, &_forward, &_up, &_breadcrumbs,
            &_location, &_places, &_entries};
        if (!_directory_mode)
            result.push_back(&_name);
        result.push_back(&_accept);
        result.push_back(&_cancel);
        return result;
    }

    void file_browser::connect_events() {
        _window.on_wnd_resize.connect([this](native::size dimensions) {
            return layout(dimensions);
        });
        _window.on_wnd_paint.connect(
            [this](native::wnd_paint_event event) {
                const native::size dimensions =
                    _window.get_dimensions();
                const int height = static_cast<int>(dimensions.h);
                event.g
                    .set_font(native::font_t::stock(
                        native::font_role::control))
                    .set_ink(native::rgba(40, 40, 40, 255))
                    .draw_text("Places", native::point(14, 49))
                    .draw_text(
                        _status,
                        native::point(
                            14,
                            static_cast<native::coord>(
                                height - 70)));
                if (!_directory_mode) {
                    event.g.draw_text(
                        "File name",
                        native::point(
                            14,
                            static_cast<native::coord>(height - 41)));
                }
                return true;
            });
        _back.on_click.connect([this] { return move_history(-1); });
        _forward.on_click.connect([this] { return move_history(1); });
        _up.on_click.connect([this] {
            const std::filesystem::path parent = _current.parent_path();
            if (!parent.empty() && parent != _current)
                navigate_to(parent, true);
            return true;
        });
        _breadcrumbs.on_navigate.connect(
            [this](const std::filesystem::path &path) {
                navigate_to(path, true);
                return true;
            });
        _places.on_selection_change.connect(
            [this](const std::vector<native::table_row_id> &rows) {
            if (!rows.empty() && valid_place(rows.back()))
                navigate_to(_place_entries[rows.back() - 1].path, true);
            return true;
        });
        _places.on_row_activate.connect([this](native::table_row_id id) {
            if (valid_place(id))
                navigate_to(_place_entries[id - 1].path, true);
            return true;
        });
        _entries.on_selection_change.connect(
            [this](const std::vector<native::table_row_id> &rows) {
                selection_changed(rows.empty() ? 0 : rows.back());
                return true;
            });
        _entries.on_row_activate.connect(
            [this](native::table_row_id id) {
                return activate_entry(id);
            });
        _accept.on_click.connect([this] { return accept(); });
        _cancel.on_click.connect([this] { return cancel(); });
        _location.on_change.connect([this](const std::string &) {
            _pending_overwrite.clear();
            return true;
        });
        _name.on_change.connect([this](const std::string &) {
            _pending_overwrite.clear();
            return true;
        });
    }

    bool file_browser::layout(native::size dimensions) {
        const int width = std::max(640, static_cast<int>(dimensions.w));
        const int height = std::max(420, static_cast<int>(dimensions.h));
        const int body_height = std::max(220, height - 154);
        const int main_width = width - 206;
        const native::rect breadcrumb_bounds(
            120, 14, static_cast<native::dim>(width - 134), 30);
        _back.set_bounds(native::rect(14, 14, 30, 30));
        _forward.set_bounds(native::rect(48, 14, 30, 30));
        _up.set_bounds(native::rect(82, 14, 30, 30));
        _breadcrumbs.set_bounds(breadcrumb_bounds);
        _location.set_bounds(_editing_location
            ? breadcrumb_bounds
            : native::rect(0, 0, 0, 0));
        _places.set_bounds(native::rect(
            14, 68, 164, static_cast<native::dim>(body_height)));
        _entries.set_bounds(native::rect(
            192, 68, static_cast<native::dim>(main_width),
            static_cast<native::dim>(body_height)));
        _places.set_column_width(1, 148);
        _entries.set_column_width(
            1, static_cast<native::dim>(std::max(120, main_width - 212)));
        _entries.set_column_width(2, 120);
        _entries.set_column_width(3, 76);
        const int action_y = height - 48;
        if (!_directory_mode) {
            _name.set_bounds(native::rect(
                82, static_cast<native::coord>(action_y),
                static_cast<native::dim>(width - 308), 30));
        }
        _accept.set_bounds(native::rect(
            static_cast<native::coord>(width - 216),
            static_cast<native::coord>(action_y), 96, 30));
        _cancel.set_bounds(native::rect(
            static_cast<native::coord>(width - 110),
            static_cast<native::coord>(action_y), 96, 30));
        _window.invalidate();
        return true;
    }

    bool file_browser::cancel() {
        _paths.clear();
        if (_window.get_created())
            _window.close(native::dialog_result::cancelled);
        return true;
    }

    void file_browser::show_location_editor(int pointer_x) {
        _editing_location = true;
        layout(_window.get_dimensions());
        if (!_window.get_created())
            return;
        const native::rect bounds = _location.get_bounds();
        const int caret_x = pointer_x >= bounds.x1() &&
                                    pointer_x < bounds.x2()
                                ? pointer_x
                                : bounds.x2() - 5;
        handle_text_edit_mouse(
            &_window, caret_x,
            bounds.p.y + static_cast<int>(bounds.d.h) / 2, true);
        handle_text_edit_mouse(
            &_window, caret_x,
            bounds.p.y + static_cast<int>(bounds.d.h) / 2, false);
    }

    void file_browser::hide_location_editor() {
        _editing_location = false;
        if (auto *state = text_edit_bindings.object_from_handle(&_location))
            state->focused = false;
        _location.set_bounds(native::rect(0, 0, 0, 0));
        SDL_StopTextInput();
        _window.invalidate();
    }

    bool file_browser::editor_focused(native::text_edit &editor) const {
        auto *state = text_edit_bindings.object_from_handle(&editor);
        return state && state->focused;
    }

    void file_browser::run_loop() {
        bool repost_quit = false;
        while (_window.get_created()) {
            SDL_Event event{};
            while (_window.get_created() && SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    cancel();
                    repost_quit = true;
                    break;
                }
                native::wnd *target = event.window.windowID
                    ? wnd_bindings.object_from_handle(
                          SDL_GetWindowFromID(event.window.windowID))
                    : nullptr;
                if (target != &_window)
                    continue;
                dispatch(event);
            }
            if (_window.get_created())
                render_window_if_needed(&_window);
            SDL_Delay(1);
        }
        SDL_StopTextInput();
        SDL_CaptureMouse(SDL_FALSE);
        if (repost_quit) {
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
    }

    void file_browser::dispatch_pointer(int x,
                                        int y,
                                        bool pressed,
                                        bool released,
                                        int clicks) {
        SDL_CaptureMouse(pressed ? SDL_TRUE : SDL_FALSE);
        const native::rect location_bounds = _editing_location
            ? _location.get_bounds()
            : _breadcrumbs.get_bounds();
        if (released && clicks == 2 &&
            location_bounds.contains(native::point(x, y))) {
            if (_editing_location)
                hide_location_editor();
            else
                show_location_editor(x);
            return;
        }
        if (handle_text_edit_mouse(&_window, x, y, pressed))
            return;
        if (handle_button_mouse(
                &_window, x, y, pressed, released))
            return;
        if (handle_collection_mouse(
                &_window, x, y, pressed, released, clicks))
            return;
        handle_canvas_mouse(
            &_window, x, y, pressed, released);
    }

    void file_browser::dispatch(const SDL_Event &event) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if ((event.key.keysym.mod & KMOD_CTRL) != 0 &&
                event.key.keysym.sym == SDLK_l) {
                show_location_editor();
            } else if ((event.key.keysym.mod & KMOD_CTRL) != 0 &&
                event.key.keysym.sym == SDLK_h) {
                _show_hidden = !_show_hidden;
                populate();
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (_editing_location)
                    hide_location_editor();
                else
                    cancel();
            } else if (event.key.keysym.sym == SDLK_RETURN ||
                       event.key.keysym.sym == SDLK_KP_ENTER) {
                if (editor_focused(_location))
                    activate_location();
                else if (!_directory_mode && editor_focused(_name))
                    accept();
                else if (!handle_collection_key(&_window, event.key))
                    accept();
            } else if (!handle_collection_key(&_window, event.key)) {
                handle_text_edit_key(&_window, event.key);
            }
            break;
        case SDL_TEXTINPUT:
            handle_text_edit_input(&_window, event.text.text);
            break;
        case SDL_MOUSEMOTION:
            update_mouse_cursor(
                &_window, native::point(event.motion.x, event.motion.y));
            if (!handle_collection_motion(
                    &_window, event.motion.x, event.motion.y))
                handle_canvas_motion(
                    &_window, event.motion.x, event.motion.y);
            handle_button_motion(
                &_window, event.motion.x, event.motion.y);
            handle_text_edit_motion(
                &_window, event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (event.button.button != SDL_BUTTON_LEFT)
                break;
            const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
            const bool released = event.type == SDL_MOUSEBUTTONUP;
            auto *state = wnd_gpx_bindings.object_from_handle(&_window);
            if (pressed && state)
                state->replay_focus_click = false;
            const bool replay = released && state &&
                                state->replay_focus_click;
            if (replay) {
                state->replay_focus_click = false;
                dispatch_pointer(
                    event.button.x, event.button.y, true, false, 1);
                if (_window.get_created()) {
                    dispatch_pointer(
                        event.button.x, event.button.y, false, true, 1);
                }
            } else {
                dispatch_pointer(
                    event.button.x, event.button.y,
                    pressed, released, event.button.clicks);
            }
            break;
        }
        case SDL_MOUSEWHEEL: {
            int x = 0;
            int y = 0;
            SDL_GetMouseState(&x, &y);
            handle_collection_wheel(
                &_window, x, y, event.wheel.y * 24);
            break;
        }
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                if (auto *state =
                        wnd_gpx_bindings.object_from_handle(&_window)) {
                    state->replay_focus_click = true;
                }
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                if (auto *state =
                        wnd_gpx_bindings.object_from_handle(&_window)) {
                    state->replay_focus_click = false;
                }
            } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                cancel();
            } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                _window.invalidate();
            } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                _window.on_native_resize(native::size(
                    static_cast<native::dim>(
                        std::max(1, event.window.data1)),
                    static_cast<native::dim>(
                        std::max(1, event.window.data2))));
            } else if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                _window.on_native_move(native::point(
                    static_cast<native::coord>(event.window.data1),
                    static_cast<native::coord>(event.window.data2)));
            }
            break;
        default:
            break;
        }
    }
} // namespace linux::sdl2
