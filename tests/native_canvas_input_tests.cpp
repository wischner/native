//
// Exercises actual SDL event dispatch into canvases nested in a panel,
// bottom tab view, and splitter. Pointer identity and capture survive
// crossing pane/menu boundaries and reset across native lifecycles.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

#include <native.h>

namespace {

// Keep event-routing assertions active in release builds.
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Resolve the SDL test window without exporting a Native handle API.
SDL_Window *find_window(const std::string &title) {
    for (Uint32 id = 1; id < 256; ++id) {
        auto *window = SDL_GetWindowFromID(id);
        if (window && title == SDL_GetWindowTitle(window))
            return window;
    }
    throw std::runtime_error("The SDL test window was not created");
}

// Recover a nested control's origin from portable parent geometry.
native::point root_origin(native::wnd &control) {
    int x = 0;
    int y = 0;
    for (auto *current = &control; current->get_parent();
         current = current->get_parent()) {
        x += current->get_position().x;
        y += current->get_position().y;
    }
    return native::point(static_cast<native::coord>(x),
                         static_cast<native::coord>(y));
}

// Queue button input through the actual main-loop SDL event adapter.
void button_event(Uint32 window, Uint8 button, bool pressed,
                  int x, int y) {
    SDL_Event event{};
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.windowID = window;
    event.button.button = button;
    event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    require(SDL_PushEvent(&event) == 1, "SDL accepts button input");
}

// Queue a pointer move in physical SDL window coordinates.
void motion_event(Uint32 window, int x, int y) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = window;
    event.motion.x = x;
    event.motion.y = y;
    require(SDL_PushEvent(&event) == 1, "SDL accepts pointer motion");
}

// Queue focus loss to exercise cancellation of an unfinished gesture.
void lose_focus(Uint32 window) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = window;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    require(SDL_PushEvent(&event) == 1, "SDL accepts focus loss");
}

// Run capture transitions in successive turns of the real event loop.
void test_canvas_dispatch() {
    native::app_wnd owner("Canvas input regression", 20, 20, 640, 480);
    native::panel panel(10, 10, 610, 420);
    native::canvas first;
    native::canvas second;
    native::split_view split(first, second,
                             native::split_orientation::vertical);
    native::tab_view tabs(4, 4, 600, 410);
    int first_cancellations = 0;
    std::vector<native::mouse_event> first_clicks;
    std::vector<native::mouse_event> second_clicks;
    std::vector<native::point> first_moves;
    std::vector<native::point> second_moves;
    owner.menu << "&File" << (native::menu_items_proxy{}
        << std::make_pair(1, "Unused test command"));
    panel.set_parent(&owner);
    tabs.set_parent(&panel);
    tabs.set_tab_placement(native::tab_placement::bottom);
    tabs.add_item("Sprite sheet", split);
    owner.on_wnd_create.connect([&] {
        panel.create();
        panel.show();
        tabs.create();
        tabs.show();
        return true;
    });
    first.on_mouse_click.connect([&](native::mouse_event event) {
        first_clicks.push_back(event);
        return true;
    });
    first.on_mouse_cancel.connect([&](native::mouse_cancel_reason reason) {
        require(reason == native::mouse_cancel_reason::focus_lost,
            "Focus cancellation carries a reason");
        ++first_cancellations;
        return false;
    });
    second.on_mouse_click.connect([&](native::mouse_event event) {
        second_clicks.push_back(event);
        return true;
    });
    first.on_mouse_move.connect([&](native::point point) {
        first_moves.push_back(point);
        return true;
    });
    second.on_mouse_move.connect([&](native::point point) {
        second_moves.push_back(point);
        return true;
    });

    std::exception_ptr failure;
    int stage = 0;
    Uint32 window_id = 0;
    int first_x = 0;
    int first_y = 0;
    int second_x = 0;
    int second_y = 0;
    const auto clear_events = [&] {
        first_clicks.clear();
        second_clicks.clear();
        first_moves.clear();
        second_moves.clear();
    };
    std::function<void()> advance;
    advance = [&] {
        try {
            switch (stage) {
            case 0: {
                auto *window = find_window(owner.get_title());
                window_id = SDL_GetWindowID(window);
                int height = 0;
                SDL_GetWindowSize(window, nullptr, &height);
                const int menu_height = height - owner.get_dimensions().h;
                require(menu_height > 0, "The fixture includes a menu bar");
                const auto first_origin = root_origin(first);
                const auto second_origin = root_origin(second);
                first_x = first_origin.x + 20;
                first_y = first_origin.y + menu_height + 20;
                second_x = second_origin.x + 20;
                second_y = second_origin.y + menu_height + 20;
                clear_events();
                button_event(window_id, SDL_BUTTON_RIGHT, true,
                             first_x, first_y);
                button_event(window_id, SDL_BUTTON_RIGHT, false,
                             first_x, first_y);
                button_event(window_id, SDL_BUTTON_MIDDLE, true,
                             second_x, second_y);
                button_event(window_id, SDL_BUTTON_MIDDLE, false,
                             second_x, second_y);
                break;
            }
            case 1:
                require(first_clicks.size() == 2 &&
                        second_clicks.size() == 2 &&
                        first_clicks[0].button == native::mouse_button::right &&
                        first_clicks[1].button == native::mouse_button::right &&
                        second_clicks[0].button == native::mouse_button::middle &&
                        second_clicks[1].button == native::mouse_button::middle,
                        "Nested canvases preserve right/middle button identity");
                require(first_clicks[0].position.x == 20 &&
                        first_clicks[0].position.y == 20,
                        "Nested input is expressed in canvas-local coordinates");
                clear_events();
                button_event(window_id, SDL_BUTTON_LEFT, true,
                             first_x, first_y);
                motion_event(window_id, second_x, second_y);
                button_event(window_id, SDL_BUTTON_LEFT, false,
                             second_x, second_y);
                break;
            case 2:
                require(first_clicks.size() == 2 &&
                        second_clicks.empty() && second_moves.empty() &&
                        !first_moves.empty() &&
                        first_moves.back().y >= first.get_dimensions().h,
                        "A pressed canvas retains drag/release across its sibling");
                clear_events();
                motion_event(window_id, second_x, second_y);
                break;
            case 3:
                require(first_moves.empty() && second_moves.size() == 1,
                        "After release pointer motion reaches the hovered sibling");
                clear_events();
                button_event(window_id, SDL_BUTTON_LEFT, true,
                             first_x, first_y);
                motion_event(window_id, first_x, 0);
                motion_event(window_id, -20, -20);
                button_event(window_id, SDL_BUTTON_LEFT, false, -20, -20);
                break;
            case 4:
                require(first_clicks.size() == 2 &&
                        first_clicks.back().position.x < 0 &&
                        first_clicks.back().position.y < 0 &&
                        first_moves.size() == 2 &&
                        first_moves.front().y < 0,
                        "Capture survives menu and negative window coordinates");
                clear_events();
                button_event(window_id, SDL_BUTTON_LEFT, true,
                             first_x, first_y);
                break;
            case 5:
                require(first_clicks.size() == 1,
                        "The first canvas is captured before destruction");
                first.destroy();
                first.create();
                first.show();
                clear_events();
                motion_event(window_id, second_x, second_y);
                button_event(window_id, SDL_BUTTON_LEFT, false,
                             second_x, second_y);
                break;
            case 6:
                require(first_clicks.empty() && first_moves.empty() &&
                        second_clicks.size() == 1 && second_moves.size() == 1,
                        "Destroy/recreate cannot retain a previous peer's capture");
                clear_events();
                button_event(window_id, SDL_BUTTON_LEFT, true,
                             first_x, first_y);
                lose_focus(window_id);
                motion_event(window_id, second_x, second_y);
                break;
            case 7:
                require(first_clicks.size() == 1 && first_cancellations == 1 &&
                        second_moves.size() == 1,
                        "Focus loss cancels once without a fabricated release");
                owner.destroy();
                return;
            default:
                throw std::logic_error("Unexpected input test stage");
            }
            ++stage;
            native::app::post(advance);
        } catch (...) {
            failure = std::current_exception();
            owner.destroy();
        }
    };
    native::app::post(advance);
    require(native::app::run(owner) == 0,
            "The SDL event loop exits normally");
    if (failure)
        std::rethrow_exception(failure);
    require(stage == 7, "Every capture transition was tested");
}

} // namespace

// Run with SDL_VIDEODRIVER=dummy and a software renderer under CTest.
int main() {
    try {
        test_canvas_dispatch();
        std::cout << "All SDL canvas input tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "SDL canvas input test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
