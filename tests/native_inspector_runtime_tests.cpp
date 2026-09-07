//
// Exercises inspector creation, server hit testing, native editor input,
// toolbar repainting, scrolling, and recreation through the application loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "inspector_input.h"

namespace
{
    using namespace native;
    using namespace std::chrono_literals;

#if defined(INSPECTOR_MOTIF)
    constexpr const char *border_names[] = {"top", "right", "bottom", "left"};
#endif

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    class fixture final : public app_wnd
    {
    public:
        fixture() : app_wnd("Inspector runtime owner", 10, 10, 120, 100),
            window(*this, "Inspector runtime", 160, 40, 500, 360),
            tools(window), second(window), bottom(window, window_edge::bottom), status(window) {
            grid.set_parent(&window);
            grid.set_items({
                {"text", "Name", property_kind::text, std::string("Old"), {}, false},
                {"number", "Number", property_kind::number, 12.0, {}, false},
                {"check", "Enabled", property_kind::boolean, false, {}, false},
                {"choice", "Mode", property_kind::choice, std::string("One"), {"One", "Two"}, false}});
            auto custom = std::make_shared<property_drop_down>();
            custom->content_size = {160, 32};
            custom->to_text = [](const property_value &value) {
                return std::to_string(std::any_cast<std::vector<int>>(std::get<std::any>(value)).size()) + " entries";
            };
            custom->create_content = [this](const property_value &, property_drop_down::commit commit)
                -> std::unique_ptr<wnd> {
                last_commit = commit;
                if (canvas_content) {
                    auto surface = std::make_unique<canvas>();
                    custom_content = surface.get();
                    surface->on_wnd_paint.connect([](wnd_paint_event event) {
                        const rgba colors[] = {{0, 0, 0, 255}, {255, 255, 255, 255},
                            {190, 40, 40, 255}, {40, 150, 60, 255}, {40, 80, 190, 255}};
                        for (int index = 0; index < 5; ++index)
                            event.g.set_ink(colors[index]).draw_rect(native::rect(index * 32, 0, 32, 32), true);
                        return true;
                    });
                    surface->on_mouse_click.connect([commit](mouse_event event) {
                        if (event.button == mouse_button::left && event.action == mouse_action::release &&
                            event.position.x >= 0 && event.position.x < 160)
                            commit(std::any(std::vector<int>(event.position.x / 32 + 1, 0)));
                        return true;
                    });
                    return surface;
                }
                auto control = std::make_unique<button>("Choose three");
                custom_content = control.get();
                control->on_click.connect([commit] { commit(std::any(std::vector<int>{1, 2, 3})); return true; });
                return control;
            };
            grid.add_item({"custom", "Custom", property_kind::drop_down,
                std::any(std::vector<int>{1}), {}, false, custom});
#if defined(INSPECTOR_MOTIF)
            for (const char *name : border_names)
                grid.add_item({name, name, property_kind::boolean, true, {}, false});
            grid.on_change.connect([this](property_change) {
                unsigned mask = 0;
                for (unsigned index = 0; index < 4; ++index)
                    if (std::get<bool>(grid.get_item(border_names[index]).value)) mask |= 1u << index;
                preview.set_border_sides(static_cast<border_sides>(mask));
                return true;
            });
#endif
            for (int index = 0; index < 20; ++index)
                grid.add_item({"extra" + std::to_string(index), "Extra",
                    property_kind::text, std::string("Value"), {}, true});
            auto icon = std::make_shared<img>(16, 16);
            icon->get_gpx().clear(rgba(0, 0, 0, 0)).set_ink(rgba(210, 20, 30, 255))
                .draw_rect(native::rect(4, 4, 8, 8), true);
            tools.set_items({{"run", "Run", tool_kind::button, {}, false, true, icon},
                {"sticky", "Sticky", tool_kind::toggle, {}, false, true, {}}});
            bottom.set_items({{"bottom", "Bottom", tool_kind::button, {}, false, true, {}}});
            second.set_items({{"second", "Second", tool_kind::button, {}, false, true, {}}});
            second.on_command.connect([this](tool_command) { ++second_commands; return true; });
            preview.set_parent(&window);
            preview.set_bounds(native::rect(355, 80, 120, 30));
            tools.on_command.connect([this](tool_command) {
                ++commands; status.set_text("Command " + std::to_string(commands)); return true;
            });
            bottom.on_command.connect([this](tool_command) { ++bottom_commands; return true; });
            window.on_wnd_create.connect([this] {
                grid.create(); grid.show();
                // Match Vision: native editors exist before final layout.
                grid.set_bounds(native::rect(10, 75, 330, 190));
                preview.create(); preview.show();
                return true;
            });
            on_wnd_create.connect([this] {
                window.create(); window.show();
                app::post([this] {
                    inspector_input::start();
                    worker = std::thread([this] { exercise(); });
                });
                return true;
            });
        }

        ~fixture() override { if (worker.joinable()) worker.join(); destroy(); }
        int result = 0;

    private:
        modeless_wnd window;
        property_grid grid;
        toolbar tools, second, bottom;
        status_bar status;
        button preview{""};
        wnd *custom_content = nullptr;
        bool canvas_content = false;
        property_drop_down::commit last_commit;
        int commands = 0, bottom_commands = 0, second_commands = 0;
        std::thread worker;

        void ui(std::function<void()> operation) {
            auto promise = std::make_shared<std::promise<void>>();
            auto future = promise->get_future();
            app::post([operation, promise] {
                try { operation(); promise->set_value(); }
                catch (...) { promise->set_exception(std::current_exception()); }
            });
            expect(future.wait_for(3s) == std::future_status::ready,
                   "application loop must remain responsive");
            future.get();
        }

        void wait_for(std::function<bool()> condition, const char *message) {
            for (int attempt = 0; attempt < 60; ++attempt) {
                std::this_thread::sleep_for(20ms);
                bool ready = false;
                ui([&] { ready = condition(); });
                if (ready) return;
            }
            throw std::runtime_error(message);
        }

        void click(point position) {
            click(window, position);
        }

        void click(app_wnd &target, point position) {
            ui([&] { inspector_input::move(target, position); });
            // Native editors may release a hidden typing cursor's pointer
            // grab in response to motion before the next physical press.
            std::this_thread::sleep_for(30ms);
            ui([&] { inspector_input::pointer(target, position, true); });
            std::this_thread::sleep_for(30ms);
            ui([&] { inspector_input::pointer(target, position, false); });
        }

        void exercise() {
            try {
                std::this_thread::sleep_for(150ms);
                int row = 0;
                ui([&] { row = grid.get_row_height(); });
                verify_pixels(row);
                click({180, static_cast<coord>(76 + 2 * row + row / 2)});
                wait_for([&] { return std::get<bool>(grid.get_item("check").value); },
                         "property checkbox must receive a real hit-tested click");
                click({215, static_cast<coord>(76 + row / 2)});
                ui([&] { inspector_input::type(window, 'z'); });
                wait_for([&] { return std::get<std::string>(grid.get_item("text").value).find('z') != std::string::npos; },
                         "property text field must accept native typing");
                click({215, static_cast<coord>(76 + row + row / 2)});
                ui([&] { inspector_input::type(window, '3'); });
                wait_for([&] { return std::get<double>(grid.get_item("number").value) != 12.0; },
                         "property number field must publish native numeric edits");
#if defined(_WIN32)
                click({315, static_cast<coord>(76 + 3 * row + row / 2)});
                click({215, static_cast<coord>(76 + 4 * row + row)});
                wait_for([&] { return std::get<std::string>(grid.get_item("choice").value) == "Two"; },
                         "property choice must retain native popup selection");
#endif
                for (int repeat = 1; repeat <= 3; ++repeat) {
                    click({20, 12});
                    wait_for([&, repeat] { return commands == repeat; },
                             "toolbar must survive repeated clicks and focus repaint");
                    verify_pixels(row);
                }
                click({90, 12});
                wait_for([&] { return tools.get_checked("sticky"); },
                         "sticky tool must retain selection");
                click({20, 345});
                wait_for([&] { return bottom_commands == 1; },
                         "bottom toolbar must receive input in its reserved strip");
                verify_sizes();
                verify_borders();
                click({215, static_cast<coord>(76 + 4 * row + row / 2)});
                app_wnd *popup = nullptr;
                wait_for([&] {
                    if (!custom_content || !custom_content->get_created()) return false;
                    popup = dynamic_cast<app_wnd *>(custom_content->get_parent());
                    return popup && popup->get_created();
                }, "custom property content must open in a real popup");
                click(*popup, {30, 20});
                wait_for([&] {
                    const auto &item = grid.get_item("custom");
                    return item.drop_down->to_text(item.value) == "3 entries" && !popup->get_created();
                }, "custom native controls commit arbitrary values through the text converter");
                property_drop_down::commit stale_commit;
                ui([&] { stale_commit = last_commit; });
                click({215, static_cast<coord>(76 + 4 * row + row / 2)});
                wait_for([&] {
                    if (!custom_content || !custom_content->get_created()) return false;
                    popup = dynamic_cast<app_wnd *>(custom_content->get_parent());
                    return popup && popup->get_created();
                }, "custom dropdown must reopen after a commit");
                ui([&] { stale_commit(std::any(std::vector<int>{1, 2, 3, 4})); });
                click(*popup, {112, 52});
                wait_for([&] {
                    const auto &item = grid.get_item("custom");
                    return !popup->get_created() && item.drop_down->to_text(item.value) == "3 entries";
                }, "Cancel and callbacks from a closed popup must preserve the committed value");
                ui([&] { canvas_content = true; });
                click({215, static_cast<coord>(76 + 4 * row + row / 2)});
                wait_for([&] {
                    if (!custom_content || !custom_content->get_created()) return false;
                    popup = dynamic_cast<app_wnd *>(custom_content->get_parent());
                    return popup && popup->get_created();
                }, "custom canvas must open after cancelling a native editor");
                click(*popup, {116, 20});
                wait_for([&] {
                    const auto &item = grid.get_item("custom");
                    return !popup->get_created() && item.drop_down->to_text(item.value) == "4 entries";
                }, "clicking a canvas colour must commit through the normal application loop");
                ui([&] { grid.set_first_visible_row(10); });
                wait_for([&] { return grid.get_first_visible_row() >= 9; },
                         "property scrolling must move the editor rows");
                ui([&] {
                    grid.set_first_visible_row(0);
                    window.destroy(); window.create(); window.show();
                });
                click({20, 12});
                wait_for([&] { return commands == 5; },
                         "toolbar input must survive owner recreation");
                std::cout << "Inspector runtime checks passed" << std::endl;
            } catch (const std::exception &error) {
                result = 1;
                std::cerr << error.what() << '\n';
            }
            app::post([this] { window.destroy(); destroy(); });
        }

        void verify_pixels(int row_height) {
            wait_for([&] {
                const auto image = inspector_input::capture(window);
                image->save("inspector-runtime.png");
                const auto *pixels = image->pixels();
                int red = 0;
                for (int y = 2; y < 22; ++y)
                    for (int x = 2; x < 75; ++x)
                        if (pixels[y * image->w() + x] == rgba(210, 20, 30, 255)) ++red;
                if (red < 32) return false;
                const auto paper = pixels[80 * image->w() + 290];
#if defined(_WIN32)
                if (pixels[(76 + 3 * row_height + row_height / 2) * image->w() + 275] != paper)
                    return false;
#endif
                int ink = 0;
                for (int y = 78; y < 76 + row_height - 2; ++y)
                    for (int x = 174; x < 270; ++x)
                        if (pixels[y * image->w() + x] != paper) ++ink;
                return ink >= 3;
            }, "presented toolbar icons and compact property text must remain visible");
        }

        void verify_sizes() {
            int previous_commands = second_commands;
            for (dim extent : {32, 24, 16}) {
                int y = 0;
                ui([&] {
                    tools.set_icon_size({extent, extent});
                    expect(tools.get_extent() == extent + 8, "icon changes resize the toolbar in both directions");
                    expect(second.get_bounds().y1() == tools.get_bounds().y2(), "adjacent toolbar follows the changed edge");
                    y = second.get_bounds().y1() + 12;
                });
                click({20, static_cast<coord>(y)});
                ++previous_commands;
                wait_for([&] { return second_commands == previous_commands; },
                         "resized neighboring toolbar must receive a native click at its new position");
            }
        }

        void verify_borders() {
#if defined(INSPECTOR_MOTIF)
            verify_motif_borders();
#endif
#if defined(__HAIKU__) || defined(INSPECTOR_SDL2)
            for (unsigned mask = 0; mask < 16; ++mask) {
                ui([&] { preview.set_border_sides(static_cast<border_sides>(mask)); });
                wait_for([&] {
                    const auto image = inspector_input::capture(window);
                    image->save("inspector-border-runtime.png");
                    const auto at = [&](int x, int y) { return image->pixels()[y * image->w() + x]; };
                    const auto paper = at(365, 90);
                    return (at(415, 80) != paper) == has_border(static_cast<border_sides>(mask), border_sides::top) &&
                        (at(415, 109) != paper) == has_border(static_cast<border_sides>(mask), border_sides::bottom) &&
                        (at(355, 95) != paper) == has_border(static_cast<border_sides>(mask), border_sides::left) &&
                        (at(474, 95) != paper) == has_border(static_cast<border_sides>(mask), border_sides::right);
                }, "all 16 live border masks must repaint their selected edges");
            }
#endif
        }

#if defined(INSPECTOR_MOTIF)
        // Click property booleans and compare the actual inset native relief.
        void verify_motif_borders() {
            int row_height = 0;
            std::unique_ptr<img> reference;
            ui([&] {
                row_height = grid.get_row_height();
                reference = inspector_input::capture(window);
            });
            const auto sample = [](const img &image, unsigned side, int depth) {
                const point positions[] = {{415, static_cast<coord>(80 + depth)},
                    {static_cast<coord>(474 - depth), 95},
                    {415, static_cast<coord>(109 - depth)},
                    {static_cast<coord>(355 + depth), 95}};
                const auto p = positions[side];
                return image.pixels()[p.y * image.w() + p.x];
            };
            const auto paper = reference->pixels()[95 * reference->w() + 415];
            for (unsigned side = 0; side < 4; ++side) {
                bool relief = false;
                for (int depth = 0; depth < 8; ++depth)
                    relief |= sample(*reference, side, depth) != paper;
                expect(relief, "native button has visible relief on every default edge");
            }
            unsigned previous = 15;
            for (unsigned step = 1; step <= 16; ++step) {
                const unsigned gray = step % 16;
                const unsigned mask = 15 ^ (gray ^ (gray >> 1));
                const unsigned changed = mask ^ previous;
                unsigned side = 0;
                while ((changed & (1u << side)) == 0) ++side;
                click({180, static_cast<coord>(76 + (5 + side) * row_height + row_height / 2)});
                wait_for([&] { return unsigned(preview.get_border_sides()) == mask; },
                         "property checkbox updates the preview border mask");
                wait_for([&] {
                    const auto image = inspector_input::capture(window);
                    image->save("inspector-border-runtime.png");
                    for (unsigned edge = 0; edge < 4; ++edge)
                        for (int depth = 0; depth < 8; ++depth) {
                            const auto expected = mask & (1u << edge)
                                ? sample(*reference, edge, depth) : paper;
                            if (sample(*image, edge, depth) != expected) return false;
                        }
                    return true;
                }, "Motif property booleans remove and restore the complete native border");
                previous = mask;
            }
        }
#endif
    };
}

int program(int, char **) {
    fixture window;
    const int status = native::app::run(window);
    return status || window.result ? 1 : 0;
}
