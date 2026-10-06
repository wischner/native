//
// Verifies actual SDL frame pixels for nested structural controls.
// Solid canvas colors must survive tab and accordion backgrounds,
// including tab replacement, splitter layout changes, and resize.
// Neighboring directional tab strips must survive viewport clip changes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "../lib/native/toolkits/sdl2/globals.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr native::rgba red(211, 29, 47, 255);
    constexpr native::rgba blue(31, 83, 219, 255);
    constexpr native::rgba green(37, 193, 71, 255);

    // Stop a failing composition check with a readable test description.
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class composition_window final : public native::app_wnd
    {
    public:
        composition_window()
            : native::app_wnd("SDL composition", 20, 20, 790, 730)
            , _split(_first, _second,
                     native::split_orientation::vertical) {
            _first.on_wnd_paint.connect(
                [](native::wnd_paint_event event) {
                    event.g.clear(red);
                    return true;
                });
            _second.on_wnd_paint.connect(
                [](native::wnd_paint_event event) {
                    event.g.clear(blue);
                    return true;
                });
            _nested.on_wnd_paint.connect(
                [](native::wnd_paint_event event) {
                    event.g.clear(green);
                    return true;
                });
            _body.on_wnd_create.connect([this] {
                _nested.set_parent(&_body);
                _nested.set_bounds(_body.get_client_bounds());
                _nested.create();
                _nested.show();
                return true;
            });
            _body.on_wnd_resize.connect([this](native::size) {
                _nested.set_bounds(_body.get_client_bounds());
                return true;
            });
            on_wnd_create.connect([this] {
                _outer.set_parent(this);
                _outer.set_bounds(native::rect(10, 10, 400, 340));
                _outer.create();
                _outer.show();
                _tabs.set_parent(&_outer);
                _tabs.set_bounds(native::rect(5, 5, 390, 330));
                _tabs.set_tab_placement(native::tab_placement::bottom);
                _tabs.add_item("Split", _split);
                _tabs.add_item("Accordion", _accordion);
                _accordion.add_item("Nested surface", _body);
                _accordion.set_expanded_index(0);
                _tabs.create();
                _tabs.show();
                _left_tabs.set_parent(this);
                _left_tabs.set_bounds(native::rect(60, 516, 300, 142));
                _left_tabs.set_tab_placement(native::tab_placement::left);
                _left_tabs.add_item("Left", _left_list);
                _left_tabs.add_item("Details", _left_details);
                _left_tabs.create();
                _left_tabs.show();
                _right_tabs.set_parent(this);
                _right_tabs.set_bounds(native::rect(400, 516, 300, 142));
                _right_tabs.set_tab_placement(native::tab_placement::right);
                _right_tabs.add_item("Right", _right_list);
                _right_tabs.add_item("Details", _right_details);
                _right_tabs.create();
                _right_tabs.show();
                native::app::post([this] { verify(); });
                return true;
            });
        }

        ~composition_window() override {
            destroy();
            _tabs.clear_items();
        }

        int result = 0;

    private:
        native::panel _outer;
        native::canvas _first;
        native::canvas _second;
        native::canvas _nested;
        native::panel _body;
        native::accordion _accordion;
        native::split_view _split;
        native::tab_view _tabs;
        native::list _left_list{{"Counter-clockwise labels"}};
        native::list _left_details{{"Details page"}};
        native::tab_view _left_tabs;
        native::list _right_list{{"Clockwise labels", "Content precedes strip"}};
        native::list _right_details{{"Details page"}};
        native::tab_view _right_tabs;
        native::ruler _horizontal_ruler{*this, native::window_edge::top, 24};
        native::ruler _vertical_ruler{*this, native::window_edge::left, 24};
        native::status_bar _status{*this};

        // Read an actual composed pixel, including all later siblings
        // and container backgrounds drawn by the SDL frame dispatcher.
        native::rgba center_pixel(native::wnd &surface) {
            auto *state = linux::sdl2::wnd_gpx_bindings
                .object_from_handle(this);
            require(state && state->renderer,
                    "The live SDL renderer is available");
            const auto origin = linux::sdl2::origin_in_root(surface);
            const auto dimensions = surface.get_dimensions();
            require(dimensions.w > 8 && dimensions.h > 8,
                    "Nested canvas has usable dimensions");
            const SDL_Rect pixel = {
                origin.x + dimensions.w / 2,
                origin.y + dimensions.h / 2 +
                    linux::sdl2::content_origin_y(this), 1, 1};
            native::rgba color;
            require(SDL_RenderReadPixels(state->renderer, &pixel,
                        SDL_PIXELFORMAT_RGBA32, &color, sizeof(color)) == 0,
                    "Read final software-renderer pixel");
            return color;
        }

        // Force the same whole-window composition used by real frames.
        void render() {
            invalidate();
            linux::sdl2::render_window_if_needed(this);
        }

        // Check actual strip pixels after the borrowed list paints.
        void verify_tab_edges() {
            _right_tabs.set_selected_index(0);
            auto appearance = native::theme::create(get_gpx());
            const auto border = appearance->get_button_border_color();
            for (auto placement : {native::tab_placement::right,
                                   native::tab_placement::top,
                                   native::tab_placement::bottom,
                                   native::tab_placement::left,
                                   native::tab_placement::right}) {
                _right_tabs.set_tab_placement(placement);
                render();
                auto *state = linux::sdl2::wnd_gpx_bindings
                    .object_from_handle(this);
                const auto origin = linux::sdl2::origin_in_root(_right_tabs);
                const auto bounds = _right_tabs.get_tab_bounds(0);
                const bool horizontal =
                    placement == native::tab_placement::top ||
                    placement == native::tab_placement::bottom;
                const int x = horizontal ? bounds.x1() + bounds.w() / 2
                    : (placement == native::tab_placement::left
                           ? bounds.x1() : bounds.x2() - 1);
                const int y = horizontal
                    ? (placement == native::tab_placement::top
                           ? bounds.y1() : bounds.y2() - 1)
                    : bounds.y1() + bounds.h() / 2;
                const SDL_Rect pixel{origin.x + x,
                    origin.y + y + linux::sdl2::content_origin_y(this),
                    1, 1};
                native::rgba color;
                require(SDL_RenderReadPixels(state->renderer, &pixel,
                            SDL_PIXELFORMAT_RGBA32, &color,
                            sizeof(color)) == 0,
                        "Read tab free-edge pixel");
                require(color == border,
                        "Tab free edge survives list painting: " +
                            std::to_string(static_cast<int>(placement)));
            }
        }

        // Exercise both directions of nesting and repeat after layout.
        void verify() {
            try {
                verify_tab_edges();
                _tabs.set_tab_placement(native::tab_placement::bottom);
                _tabs.set_selected_index(0);
                render();
                require(center_pixel(_first) == red,
                        "Bottom tabs retain the upper split canvas");
                require(center_pixel(_second) == blue,
                        "Bottom tabs retain the lower split canvas");
                _tabs.set_selected_index(1);
                render();
                require(center_pixel(_nested) == green,
                        "Accordion retains its nested panel canvas");
                _tabs.set_selected_index(0);
                _outer.set_dimensions(native::size(350, 290));
                _tabs.set_dimensions(native::size(340, 280));
                _split.set_ratio(0.35F);
                render();
                require(center_pixel(_first) == red &&
                            center_pixel(_second) == blue,
                        "Resized split canvases survive switching back");
                _tabs.set_selected_index(1);
                render();
                require(center_pixel(_nested) == green,
                        "Recreated nested canvas retains its image");
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n';
                result = 1;
            }
            destroy();
        }
    };
} // namespace

int program(int, char **) {
    composition_window window;
    const int status = native::app::run(window);
    if (SDL_WasInit(0) != 0) {
        std::cerr << "FAILED: SDL services remain initialized after exit\n";
        return 1;
    }
    return status ? status : window.result;
}
