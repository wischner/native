//
// Implements a self-contained SDL file chooser for installations without
// a desktop-native chooser process.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_fallback.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fnmatch.h>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

#include <native/app_wnd.h>

#include "globals.h"
#include "window_position.h"

namespace
{
    namespace fs = std::filesystem;

    constexpr int chooser_width = 640;
    constexpr int chooser_height = 520;
    constexpr int margin = 14;
    constexpr int list_top = 54;
    constexpr int list_height = 338;
    constexpr int row_height = 24;
    constexpr int path_top = 404;
    constexpr int path_height = 32;
    constexpr int button_top = 452;
    constexpr int button_height = 34;

    struct entry
    {
        std::string label;
        fs::path path;
        bool directory = false;
    };

    struct chooser_state
    {
        const native::file_dialog *dialog = nullptr;
        fs::path directory;
        std::vector<entry> entries;
        std::set<int> selected;
        std::string path_text;
        std::string status;
        std::string default_extension;
        std::string pending_overwrite;
        int current = -1;
        int first_row = 0;
        int last_clicked = -1;
        Uint32 last_click_time = 0;
        bool save = false;
        bool allow_multiple = false;
        bool confirm_overwrite = true;
        bool path_focused = false;
        bool select_all_path = false;
        bool done = false;
        bool accepted = false;
    };

    bool contains(const SDL_Rect &bounds, int x, int y) {
        return x >= bounds.x && y >= bounds.y &&
               x < bounds.x + bounds.w && y < bounds.y + bounds.h;
    }

    std::string folded(std::string value) {
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(
                               std::tolower(character));
                       });
        return value;
    }

    bool matches_filters(const native::file_dialog &dialog,
                         const std::string &name) {
        if (dialog.get_filters().empty())
            return true;
        const std::string candidate = folded(name);
        for (const native::file_filter &filter : dialog.get_filters()) {
            for (const std::string &pattern : filter.patterns) {
                const std::string wildcard = folded(pattern);
                if (fnmatch(wildcard.c_str(),
                            candidate.c_str(),
                            0) == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    void ensure_visible(chooser_state &state) {
        const int rows = list_height / row_height;
        if (state.current < state.first_row)
            state.first_row = state.current;
        if (state.current >= state.first_row + rows)
            state.first_row = state.current - rows + 1;
        const int maximum = std::max(
            0, static_cast<int>(state.entries.size()) - rows);
        state.first_row = std::clamp(state.first_row, 0, maximum);
    }

    void populate(chooser_state &state) {
        state.entries.clear();
        state.selected.clear();
        state.current = -1;
        state.first_row = 0;
        const fs::path parent = state.directory.parent_path();
        if (!parent.empty() && parent != state.directory)
            state.entries.push_back({"[..]", parent, true});

        std::error_code error;
        fs::directory_iterator iterator(state.directory, error);
        const fs::directory_iterator end;
        while (!error && iterator != end) {
            const fs::directory_entry item = *iterator;
            iterator.increment(error);
            std::error_code type_error;
            const bool directory = item.is_directory(type_error);
            if (type_error)
                continue;
            const std::string name = item.path().filename().string();
            if (!directory &&
                !matches_filters(*state.dialog, name)) {
                continue;
            }
            state.entries.push_back(
                {directory ? "[" + name + "]" : name,
                 item.path(),
                 directory});
        }

        std::sort(state.entries.begin(),
                  state.entries.end(),
                  [](const entry &left, const entry &right) {
                      if (left.label == "[..]" &&
                          right.label != "[..]") {
                          return true;
                      }
                      if (right.label == "[..]")
                          return false;
                      if (left.directory != right.directory)
                          return left.directory > right.directory;
                      return folded(left.label) < folded(right.label);
                  });
        state.status = error ? "Unable to read this folder."
                             : state.directory.string();
    }

    void enter_directory(chooser_state &state,
                         const fs::path &directory) {
        std::error_code error;
        fs::path absolute = fs::absolute(directory, error);
        if (error || !fs::is_directory(absolute, error) || error) {
            state.status = "The selected folder is unavailable.";
            return;
        }
        state.directory = absolute.lexically_normal();
        state.pending_overwrite.clear();
        populate(state);
        if (!state.save)
            state.path_text.clear();
    }

    void select_entry(chooser_state &state,
                      int index,
                      bool extend) {
        if (index < 0 ||
            index >= static_cast<int>(state.entries.size())) {
            return;
        }
        state.current = index;
        if (!extend || !state.allow_multiple)
            state.selected.clear();
        if (state.allow_multiple && extend &&
            state.selected.contains(index)) {
            state.selected.erase(index);
        } else {
            state.selected.insert(index);
        }
        state.path_text = state.entries[index].path.string();
        state.path_focused = false;
        state.select_all_path = false;
        state.pending_overwrite.clear();
        ensure_visible(state);
    }

    void accept(chooser_state &state) {
        if (state.allow_multiple && !state.save &&
            state.selected.size() > 1) {
            for (int index : state.selected) {
                if (index >= 0 &&
                    index < static_cast<int>(state.entries.size()) &&
                    state.entries[index].directory) {
                    state.status =
                        "Multiple selection accepts files only.";
                    return;
                }
            }
            state.accepted = true;
            state.done = true;
            return;
        }

        fs::path path = state.path_text;
        if (path.empty() && state.current >= 0)
            path = state.entries[state.current].path;
        if (path.empty()) {
            state.status = "Enter or select a file name.";
            return;
        }
        if (path.is_relative())
            path = state.directory / path;
        path = path.lexically_normal();

        std::error_code error;
        if (fs::is_directory(path, error) && !error) {
            enter_directory(state, path);
            return;
        }
        if (!state.save) {
            if (!fs::is_regular_file(path, error) || error) {
                state.status = "The selected file does not exist.";
                return;
            }
        } else {
            path = linux::add_default_extension(
                path.string(), state.default_extension);
            const fs::path parent = path.parent_path().empty()
                                        ? state.directory
                                        : path.parent_path();
            if (!fs::is_directory(parent, error) || error) {
                state.status =
                    "The destination folder does not exist.";
                return;
            }
            if (state.confirm_overwrite && fs::exists(path, error) &&
                !error && state.pending_overwrite != path.string()) {
                state.pending_overwrite = path.string();
                state.path_text = path.string();
                state.status =
                    "File exists; choose Save again to replace it.";
                return;
            }
        }
        state.path_text = path.string();
        state.accepted = true;
        state.done = true;
    }

    void fill(SDL_Renderer *renderer,
              const SDL_Rect &bounds,
              SDL_Color color) {
        SDL_SetRenderDrawColor(
            renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &bounds);
    }

    void frame(SDL_Renderer *renderer,
               const SDL_Rect &bounds,
               SDL_Color color) {
        SDL_SetRenderDrawColor(
            renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawRect(renderer, &bounds);
    }

    void label(SDL_Renderer *renderer,
               const std::string &text,
               const SDL_Rect &bounds,
               SDL_Color color,
               bool centered = false) {
        SDL_RenderSetClipRect(renderer, &bounds);
        const int x = centered
                          ? bounds.x + std::max(
                                           0,
                                           (bounds.w -
                                            linux::sdl2::text_width(text)) /
                                               2)
                          : bounds.x + 5;
        const int y = bounds.y + std::max(
                                   0,
                                   (bounds.h -
                                    linux::sdl2::text_height()) /
                                       2);
        linux::sdl2::draw_text(renderer, text, x, y, color);
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    SDL_Rect button_rect(int x, int width) {
        return SDL_Rect{x, button_top, width, button_height};
    }

    void draw(chooser_state &state, SDL_Renderer *renderer) {
        constexpr SDL_Color paper{242, 242, 242, 255};
        constexpr SDL_Color white{255, 255, 255, 255};
        constexpr SDL_Color ink{30, 30, 30, 255};
        constexpr SDL_Color edge{100, 100, 100, 255};
        constexpr SDL_Color selected{49, 106, 197, 255};
        constexpr SDL_Color selected_text{255, 255, 255, 255};
        SDL_SetRenderDrawColor(
            renderer, paper.r, paper.g, paper.b, paper.a);
        SDL_RenderClear(renderer);

        label(renderer,
              state.status,
              SDL_Rect{margin, 12, chooser_width - margin * 2, 30},
              ink);
        const SDL_Rect list_bounds{
            margin, list_top, chooser_width - margin * 2, list_height};
        fill(renderer, list_bounds, white);
        frame(renderer, list_bounds, edge);
        const int rows = list_height / row_height;
        for (int row = 0; row < rows; ++row) {
            const int index = state.first_row + row;
            if (index >= static_cast<int>(state.entries.size()))
                break;
            const SDL_Rect row_bounds{
                margin + 1,
                list_top + 1 + row * row_height,
                chooser_width - margin * 2 - 2,
                row_height};
            const bool chosen = state.selected.contains(index);
            fill(renderer, row_bounds, chosen ? selected : white);
            label(renderer,
                  state.entries[index].label,
                  row_bounds,
                  chosen ? selected_text : ink);
        }

        const SDL_Rect path_bounds{
            margin, path_top, chooser_width - margin * 2, path_height};
        fill(renderer, path_bounds, white);
        frame(renderer,
              path_bounds,
              state.path_focused ? selected : edge);
        label(renderer, state.path_text, path_bounds, ink);

        const SDL_Rect up = button_rect(margin, 82);
        const SDL_Rect action = button_rect(438, 88);
        const SDL_Rect cancel = button_rect(536, 90);
        for (const SDL_Rect &button : {up, action, cancel}) {
            fill(renderer, button, paper);
            frame(renderer, button, edge);
        }
        label(renderer, "Up", up, ink, true);
        label(renderer,
              state.save ? "Save" : "Open",
              action,
              ink,
              true);
        label(renderer, "Cancel", cancel, ink, true);
        SDL_RenderPresent(renderer);
    }

    void backspace(std::string &text) {
        if (text.empty())
            return;
        text.pop_back();
        while (!text.empty() &&
               (static_cast<unsigned char>(text.back()) & 0xc0) == 0x80) {
            text.pop_back();
        }
    }

    void handle_key(chooser_state &state,
                    const SDL_KeyboardEvent &event) {
        const SDL_Keycode key = event.keysym.sym;
        const bool control =
            (event.keysym.mod & KMOD_CTRL) != 0;
        if (key == SDLK_ESCAPE) {
            state.done = true;
        } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            accept(state);
        } else if (state.path_focused && key == SDLK_BACKSPACE) {
            if (state.select_all_path)
                state.path_text.clear();
            else
                backspace(state.path_text);
            state.select_all_path = false;
        } else if (state.path_focused && control && key == SDLK_a) {
            state.select_all_path = true;
        } else if (state.path_focused && control && key == SDLK_v) {
            char *clipboard = SDL_GetClipboardText();
            if (clipboard) {
                if (state.select_all_path)
                    state.path_text.clear();
                state.path_text += clipboard;
                state.select_all_path = false;
                SDL_free(clipboard);
            }
        } else if (!state.path_focused &&
                   (key == SDLK_UP || key == SDLK_DOWN)) {
            const int delta = key == SDLK_UP ? -1 : 1;
            const int next = std::clamp(
                state.current < 0 ? 0 : state.current + delta,
                0,
                std::max(0,
                         static_cast<int>(state.entries.size()) - 1));
            select_entry(state, next, false);
        }
    }

    void handle_mouse(chooser_state &state,
                      const SDL_MouseButtonEvent &event) {
        if (event.button != SDL_BUTTON_LEFT)
            return;
        const SDL_Rect list_bounds{
            margin, list_top, chooser_width - margin * 2, list_height};
        const SDL_Rect path_bounds{
            margin, path_top, chooser_width - margin * 2, path_height};
        const SDL_Rect up = button_rect(margin, 82);
        const SDL_Rect action = button_rect(438, 88);
        const SDL_Rect cancel = button_rect(536, 90);
        if (contains(list_bounds, event.x, event.y)) {
            const int row = (event.y - list_top - 1) / row_height;
            const int index = state.first_row + row;
            if (index >= 0 &&
                index < static_cast<int>(state.entries.size())) {
                const Uint32 now = SDL_GetTicks();
                const bool double_click =
                    state.last_clicked == index &&
                    now - state.last_click_time <= 500;
                select_entry(
                    state,
                    index,
                    (SDL_GetModState() & KMOD_CTRL) != 0);
                state.last_clicked = index;
                state.last_click_time = now;
                if (double_click) {
                    if (state.entries[index].directory)
                        enter_directory(state,
                                        state.entries[index].path);
                    else
                        accept(state);
                }
            }
            return;
        }
        if (contains(path_bounds, event.x, event.y)) {
            state.path_focused = true;
            state.select_all_path = false;
        } else if (contains(up, event.x, event.y)) {
            enter_directory(state, state.directory.parent_path());
        } else if (contains(action, event.x, event.y)) {
            accept(state);
        } else if (contains(cancel, event.x, event.y)) {
            state.done = true;
        }
    }
} // namespace

namespace linux::sdl2
{
    linux::file_dialog_response show_file_dialog_fallback(
        const native::file_dialog &dialog,
        bool save,
        bool allow_multiple,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite) {
        linux::file_dialog_response response;
        native::app_wnd *owner = dialog.get_owner();
        SDL_Window *owner_window = owner
            ? wnd_bindings.handle_from_object(owner)
            : nullptr;
        if (!owner_window)
            return response;

        chooser_state state;
        state.dialog = &dialog;
        state.save = save;
        state.allow_multiple = allow_multiple;
        state.default_extension = default_extension;
        state.confirm_overwrite = confirm_overwrite;
        state.path_text = suggested_name;
        std::error_code error;
        fs::path initial = dialog.get_initial_path();
        if (initial.empty())
            initial = fs::current_path(error);
        if (error)
            initial = ".";
        if (!fs::is_directory(initial, error) || error) {
            if (!initial.filename().empty() && state.path_text.empty())
                state.path_text = initial.filename().string();
            initial = initial.parent_path();
        }
        if (initial.empty())
            initial = ".";
        state.directory = fs::absolute(initial, error);
        if (error)
            state.directory = ".";
        populate(state);

        int owner_x = 0;
        int owner_y = 0;
        int owner_width = 0;
        int owner_height = 0;
        SDL_GetWindowPosition(owner_window, &owner_x, &owner_y);
        SDL_GetWindowSize(owner_window, &owner_width, &owner_height);
        const native::point desired(
            owner_x + std::max(0, (owner_width - chooser_width) / 2),
            owner_y + std::max(0, (owner_height - chooser_height) / 2));
        const native::point position = constrain_window_position(
            nullptr,
            desired,
            native::size(chooser_width, chooser_height));
        SDL_Window *window = SDL_CreateWindow(
            dialog.get_title().c_str(),
            position.x,
            position.y,
            chooser_width,
            chooser_height,
            SDL_WINDOW_SHOWN);
        if (!window)
            return response;
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetWindowModalFor(window, owner_window);
#endif
        SDL_Renderer *renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer)
            renderer = SDL_CreateRenderer(window,
                                          -1,
                                          SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            SDL_DestroyWindow(window);
            return response;
        }

        SDL_RaiseWindow(window);
        SDL_SetWindowInputFocus(window);
        SDL_StartTextInput();
        const Uint32 window_id = SDL_GetWindowID(window);
        while (!state.done) {
            draw(state, renderer);
            SDL_Event event{};
            if (!SDL_WaitEventTimeout(&event, 100))
                continue;
            if (event.type == SDL_QUIT) {
                SDL_PushEvent(&event);
                state.done = true;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.windowID == window_id &&
                       event.window.event == SDL_WINDOWEVENT_CLOSE) {
                state.done = true;
            } else if (event.type == SDL_KEYDOWN &&
                       event.key.windowID == window_id) {
                handle_key(state, event.key);
            } else if (event.type == SDL_TEXTINPUT &&
                       event.text.windowID == window_id &&
                       state.path_focused) {
                if (state.select_all_path)
                    state.path_text.clear();
                state.path_text += event.text.text;
                state.select_all_path = false;
                state.pending_overwrite.clear();
            } else if (event.type == SDL_MOUSEBUTTONUP &&
                       event.button.windowID == window_id) {
                handle_mouse(state, event.button);
            } else if (event.type == SDL_MOUSEWHEEL &&
                       event.wheel.windowID == window_id) {
                state.first_row -= event.wheel.y * 3;
                const int rows = list_height / row_height;
                const int maximum = std::max(
                    0,
                    static_cast<int>(state.entries.size()) - rows);
                state.first_row = std::clamp(
                    state.first_row, 0, maximum);
            }
        }
        SDL_StopTextInput();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_RaiseWindow(owner_window);

        if (!state.accepted) {
            response.outcome = linux::file_dialog_outcome::cancelled;
            return response;
        }
        if (state.allow_multiple && !state.save &&
            state.selected.size() > 1) {
            for (int index : state.selected) {
                if (index >= 0 &&
                    index < static_cast<int>(state.entries.size())) {
                    response.paths.push_back(
                        state.entries[index].path.string());
                }
            }
        } else {
            response.paths.push_back(state.path_text);
        }
        response.outcome = linux::file_dialog_outcome::accepted;
        return response;
    }
} // namespace linux::sdl2
