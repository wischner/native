//
// Drives real Rasta packets through the application loop, not editor helpers.
// Covers focus, editing, clipboard, and collection scrollbar input in both
// GEM transports, including nested controls in a modeless window.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/classic_scrollbar.h"
#include "../lib/native/toolkits/gemix/globals.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    void expect(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }

    class input_window final : public native::app_wnd
    {
    public:
        input_window() : native::app_wnd("Input regression", 36, 36, 440, 260),
            field("", native::text_edit_mode::single_line, 20, 20, 300, 26),
            multi("", native::text_edit_mode::multi_line, 20, 65, 300, 80),
            copy("Copy field", 20, 160, 120, 28),
            paste("Paste text", 160, 160, 120, 28),
            collections(*this, "Collection input", 60, 330, 620, 380),
            icons({}, 0, 0, 270, 240),
            libraries(20, 20, 270, 300),
            tree({}, 320, 20, 260, 300),
            inspector(*this, "Inspector input", 60, 330, 420, 260),
            properties(10, 40, 330, 100), tools(inspector) {
            menu << "Edit" << (native::menu_items("Copy\tCtrl+C")
                << "Paste\tCtrl+V");
            on_wnd_create.connect(this, &input_window::children);
            copy.on_click.connect([this] {
                field.select_all(); field.copy(); return true;
            });
            paste.on_click.connect([this] {
                multi.select_all(); multi.paste(); return true;
            });
            input = socket(AF_INET, SOCK_DGRAM, 0);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(5012);
            timeval timeout{3, 0};
            expect(input >= 0 && bind(input, reinterpret_cast<sockaddr *>(&address),
                sizeof(address)) == 0, "bind test viewer");
            setsockopt(input, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        }

        ~input_window() {
            if (worker.joinable()) worker.join();
            close(input);
        }

        int failures = 0;

    private:
        native::text_edit field, multi;
        native::button copy, paste;
        native::modeless_wnd collections;
        native::icon_view icons;
        native::accordion libraries;
        native::tree_view tree;
        native::modeless_wnd inspector;
        native::property_grid properties;
        native::toolbar tools;
        native::wnd *custom_content = nullptr;
        int property_changes = 0, tool_actions = 0;
        int collection_actions = 0;
        std::thread worker;
        int input = -1;
        sockaddr_in peer{};
        native::point origin;

        bool children() {
            for (native::wnd *child : {static_cast<native::wnd *>(&field),
                    static_cast<native::wnd *>(&multi),
                    static_cast<native::wnd *>(&copy),
                    static_cast<native::wnd *>(&paste)}) {
                child->set_parent(this); child->create(); child->show();
            }
            native::app::post([this] {
                origin = linux::gemix::work_rect(
                    linux::gemix::wnd_bindings.handle_from_object(this)).p;
                worker = std::thread([this] { exercise(); });
            });
            return true;
        }

        void packet(unsigned type, int first, int second = 0) {
            uint16_t data[3] = {htons(type), htons(first), htons(second)};
            expect(sendto(input, data, sizeof(data), 0,
                reinterpret_cast<sockaddr *>(&peer), sizeof(peer)) == sizeof(data),
                "send test input");
        }

        void click(int x, int y) {
            packet(3, origin.x + x, origin.y + y);
            packet(10, origin.x + x, origin.y + y);
            packet(11, origin.x + x, origin.y + y);
        }

        void pointer(unsigned type, native::point position) {
            packet(type, origin.x + position.x, origin.y + position.y);
        }

        void press(native::point position) {
            pointer(3, position);
            pointer(10, position);
        }

        native::point center(const native::rect &bounds) {
            return native::point(bounds.x1() + bounds.w() / 2,
                                 bounds.y1() + bounds.h() / 2);
        }

        void key(unsigned scan) { packet(1, scan); packet(2, scan); }

        void shortcut(unsigned scan) {
            packet(1, 224); key(scan); packet(2, 224);
        }

        // Read portable values on the UI thread while real input is dispatched.
        void wait_for(std::function<bool()> condition, const char *message) {
            for (int attempt = 0; attempt < 40; ++attempt) {
                std::this_thread::sleep_for(25ms);
                auto result = std::make_shared<std::promise<bool>>();
                auto future = result->get_future();
                native::app::post([condition, result] { result->set_value(condition()); });
                expect(future.wait_for(2s) == std::future_status::ready,
                       "application loop stopped responding");
                if (future.get()) return;
            }
            throw std::runtime_error(message);
        }

        // Locate the painted parts on the UI thread; packets still pass
        // through the complete Rasta/AES event loop before assertions.
        struct scrollbar_state
        {
            native::detail::classic_scrollbar_geometry parts;
            int maximum = 0;
            int page = 0;
        };

        scrollbar_state scrollbar(native::collection_view &control) {
            scrollbar_state result;
            wait_for([&] {
                const auto dimensions = control.get_dimensions();
                int content = 0;
                if (auto *grid = dynamic_cast<native::icon_view *>(&control))
                    content = grid->get_content_dimensions().h;
                else {
                    auto &outline = static_cast<native::tree_view &>(control);
                    content = outline.get_visible_item_count() *
                        outline.get_row_bounds(0).h();
                }
                const auto metrics = native::theme::create(
                    control.get_gpx())->defaults();
                const int extent = std::max(1, metrics.scrollbar_extent);
                const auto position = linux::gemix::origin_in_root(control);
                const native::rect bounds(position.x + dimensions.w - extent,
                    position.y, extent, dimensions.h);
                result.page = dimensions.h;
                result.maximum = content - result.page;
                result.parts = native::detail::make_classic_scrollbar(
                    bounds, native::scrollbar_orientation::vertical,
                    content, result.page, control.get_scroll_offset(),
                    metrics.scrollbar_min_thumb);
                return true;
            }, "read collection scrollbar geometry");
            expect(result.maximum > result.page,
                   "collection fixture overflows by more than one page");
            return result;
        }

        void check_scrollbar(native::collection_view &control) {
            const auto initial = scrollbar(control);
            const auto down = center(initial.parts.increment);
            press(down);
            wait_for([&] { return control.get_scroll_offset() > 0; },
                     &control == &icons
                         ? "icon down arrow must scroll on press"
                         : "tree down arrow must scroll on press");
            int step = 0;
            wait_for([&] { step = control.get_scroll_offset(); return true; },
                     "read arrow scroll amount");
            pointer(11, down);
            wait_for([&] {
                return control.get_scroll_offset() == step &&
                    collection_actions == 0;
            }, "scrollbar release changes neither offset nor selection");

            const auto up = center(initial.parts.decrement);
            press(up);
            wait_for([&] { return control.get_scroll_offset() == 0; },
                     "collection up arrow must scroll on press");
            pointer(11, up);

            const native::point page_down(down.x,
                initial.parts.trough.y2() - 2);
            press(page_down);
            wait_for([&] {
                return control.get_scroll_offset() == initial.page;
            }, "collection trough must page down on press");
            pointer(11, page_down);
            auto current = scrollbar(control);
            const native::point page_up(up.x, current.parts.trough.y1() + 1);
            press(page_up);
            wait_for([&] { return control.get_scroll_offset() == 0; },
                     "collection trough must page up on press");
            pointer(11, page_up);

            const auto thumb = center(initial.parts.thumb);
            press(thumb);
            wait_for([&] {
                return linux::gemix::window_states.object_from_handle(
                    &collections)->capture == &control;
            }, "collection thumb captures the pointer");
            // Leave both the child and its modeless AES work area.
            const native::point beyond_bottom(thumb.x, 450);
            pointer(3, beyond_bottom);
            wait_for([&] {
                return control.get_scroll_offset() == initial.maximum;
            }, "captured collection thumb reaches the bottom outside its window");
            const native::point beyond_top(thumb.x, -80);
            pointer(3, beyond_top);
            wait_for([&] { return control.get_scroll_offset() == 0; },
                     "captured collection thumb reaches the top outside its window");
            pointer(11, beyond_top);
            wait_for([&] {
                return !linux::gemix::window_states.object_from_handle(
                    &collections)->capture && collection_actions == 0;
            }, "collection release clears capture without item actions");
            pointer(3, beyond_bottom);
            wait_for([&] { return control.get_scroll_offset() == 0; },
                     "pointer movement after release does not scroll");
        }

        void check_collections() {
            wait_for([this] {
                for (int index = 0; index < 64; ++index) {
                    icons.add_item({"Icon " + std::to_string(index), nullptr,
                        static_cast<std::uint64_t>(index + 1), true});
                    tree.add_item(native::tree_node(
                        "Row " + std::to_string(index), index + 1));
                }
                libraries.add_item("Icons", icons);
                libraries.set_expanded_index(0);
                collections.create();
                libraries.set_parent(&collections);
                libraries.create(); libraries.show();
                tree.set_parent(&collections);
                tree.create(); tree.show();
                collections.show();
                icons.set_selected_index(0);
                tree.set_selected_item(1);
                icons.on_selection_change.connect([this](int) {
                    ++collection_actions; return true;
                });
                icons.on_item_activate.connect([this](int) {
                    ++collection_actions; return true;
                });
                tree.on_selection_change.connect([this](native::tree_item_id) {
                    ++collection_actions; return true;
                });
                tree.on_item_activate.connect([this](native::tree_item_id) {
                    ++collection_actions; return true;
                });
                origin = linux::gemix::work_rect(
                    linux::gemix::wnd_bindings.handle_from_object(&collections)).p;
                return true;
            }, "create modeless collection controls");
            check_scrollbar(icons);
            check_scrollbar(tree);
            wait_for([this] {
                return icons.get_selected_index() == 0 &&
                    tree.get_selected_item() == 1 && collection_actions == 0;
            }, "scrollbar gestures preserve selected collection items");

            // Destroy each borrowed control while its thumb is held.
            for (native::collection_view *control : {
                    static_cast<native::collection_view *>(&icons),
                    static_cast<native::collection_view *>(&tree)}) {
                const auto thumb = center(scrollbar(*control).parts.thumb);
                press(thumb);
                wait_for([&] {
                    return linux::gemix::window_states.object_from_handle(
                        &collections)->capture == control;
                }, "collection captures before destruction");
                wait_for([&] {
                    // Capture still belongs to the original root when a
                    // borrowed collection changes parent before destruction.
                    if (control == &icons) control->set_parent(this);
                    control->destroy();
                    return !linux::gemix::window_states.object_from_handle(
                        &collections)->capture;
                }, "destroyed collection releases pointer capture");
                pointer(11, thumb);
            }
            wait_for([this] {
                collections.destroy();
                origin = linux::gemix::work_rect(
                    linux::gemix::wnd_bindings.handle_from_object(this)).p;
                return true;
            }, "close collection window");
        }

        // Exercise real packets through composite children and toolbar capture.
        void check_inspector() {
            int row_height = 0;
            wait_for([this, &row_height] {
                using native::property_kind;
                using native::tool_kind;
                properties.set_items({
                    {"name", "Name", property_kind::text, std::string("Old"), {}, false},
                    {"size", "Size", property_kind::number, 12.0, {}, false},
                    {"visible", "Visible", property_kind::boolean, false, {}, false},
                    {"mode", "Mode", property_kind::choice, std::string("One"), {"One", "Two"}, false}});
                auto custom = std::make_shared<native::property_drop_down>();
                custom->content_size = {120, 32};
                custom->to_text = [](const native::property_value &value) {
                    return std::to_string(std::any_cast<int>(std::get<std::any>(value)));
                };
                custom->create_content = [this](const native::property_value &,
                    native::property_drop_down::commit commit) {
                    auto canvas = std::make_unique<native::canvas>();
                    custom_content = canvas.get();
                    canvas->on_mouse_click.connect([commit](native::mouse_event event) {
                        if (event.action == native::mouse_action::release) commit(std::any(24));
                        return true;
                    });
                    return canvas;
                };
                properties.add_item({"custom", "Custom", property_kind::drop_down,
                    std::any(16), {}, false, custom});
                for (int index = 0; index < 20; ++index)
                    properties.add_item({"extra" + std::to_string(index), "Extra",
                        property_kind::text, std::string("Value"), {}, false});
                tools.set_items({{"run", "Run", tool_kind::button, {}, false, true, {}},
                    {"select", "Select", tool_kind::exclusive, "tools", true, true, {}},
                    {"draw", "Draw", tool_kind::exclusive, "tools", false, true, {}}});
                tools.on_command.connect([this](native::tool_command) { ++tool_actions; return true; });
                properties.on_change.connect([this](native::property_change) { ++property_changes; return true; });
                inspector.create();
                properties.set_parent(&inspector); properties.create(); properties.show();
                inspector.show();
                origin = linux::gemix::work_rect(linux::gemix::wnd_bindings.handle_from_object(&inspector)).p;
                row_height = properties.get_row_height();
                return row_height > 0 && row_height <= 24;
            }, "create compact property grid and toolbar");
            const auto row_center = [row_height](int index) {
                return 41 + index * row_height + row_height / 2;
            };
            click(210, row_center(0)); shortcut(4); key(4);
            wait_for([this] { return std::get<std::string>(properties.get_item("name").value) == "a"; },
                "property text editor receives real typing");
            click(210, row_center(1)); shortcut(4); key(31); key(32);
            wait_for([this] { return std::get<double>(properties.get_item("size").value) == 23.0; },
                "property number editor commits typed value");
            click(210, row_center(2));
            wait_for([this] { return std::get<bool>(properties.get_item("visible").value); },
                "property boolean checkbox receives clicks");
            click(310, row_center(3)); click(210, 41 + 4 * row_height + 30);
            wait_for([this] { return std::get<std::string>(properties.get_item("mode").value) == "Two"; },
                "property combo popup selects a value beyond the grid viewport");
            click(20, 12);
            wait_for([this] { return tool_actions == 1; }, "toolbar push button receives click");
            click(120, 12);
            wait_for([this] { return tools.get_checked("draw") && !tools.get_checked("select") && tool_actions == 2; },
                "toolbar exclusive button selects one tool");
            press({20, 12}); pointer(3, {20, 210}); pointer(11, {20, 210});
            wait_for([this] {
                return !linux::gemix::window_states.object_from_handle(&inspector)->capture && tool_actions == 2;
            }, "toolbar release outside cancels activation and clears capture");
            wait_for([this] { properties.set_dimensions({330, 140}); return true; },
                "expose custom dropdown row");
            click(210, row_center(4));
            native::app_wnd *popup = nullptr;
            wait_for([this, &popup] {
                if (!custom_content || !custom_content->get_created()) return false;
                popup = dynamic_cast<native::app_wnd *>(custom_content->get_parent());
                if (!popup || !popup->get_created()) return false;
                origin = linux::gemix::work_rect(linux::gemix::wnd_bindings.handle_from_object(popup)).p;
                return true;
            }, "custom canvas opens in a GEM popup");
            click(20, 20);
            wait_for([this, popup] {
                if (std::any_cast<int>(std::get<std::any>(properties.get_item("custom").value)) != 24 ||
                    popup->get_created()) return false;
                properties.set_dimensions({330, 100});
                origin = linux::gemix::work_rect(linux::gemix::wnd_bindings.handle_from_object(&inspector)).p;
                return true;
            }, "GEM custom canvas commits through the value converter");
            click(332, 131); click(332, 131);
            wait_for([this] { return properties.get_first_visible_row() > 0; },
                "property grid scrollbar moves rows");
            wait_for([this] {
                properties.set_first_visible_row(0);
                expect(property_changes >= 4, "property editing emits user changes");
                expect(std::get<double>(properties.get_item("size").value) == 23.0,
                    "scrolling preserves edited properties");
                inspector.destroy();
                inspector.create(); properties.create(); properties.show(); inspector.show();
                inspector.destroy();
                origin = linux::gemix::work_rect(linux::gemix::wnd_bindings.handle_from_object(this)).p;
                return true;
            }, "composite controls survive destroy and recreate");
        }

        void exercise() {
            try {
                char subscription[4096];
                socklen_t size = sizeof(peer);
                expect(recvfrom(input, subscription, sizeof(subscription), 0,
                    reinterpret_cast<sockaddr *>(&peer), &size) > 0,
                    "receive Rasta subscription");
                click(40, 30); key(4); key(5); key(6);
                wait_for([this] { return field.get_text() == "abc"; },
                         "single-line field lost click or typed keys");
                click(40, 75); key(27); key(40); key(28);
                wait_for([this] { return multi.get_text() == "x\ny"; },
                         "multiline field lost click, text or Enter");
                click(50, 173); click(200, 173);
                wait_for([this] { return multi.get_text() == "abc"; },
                         "Copy field/Paste text buttons failed");
                click(40, 30); shortcut(4); key(7); key(8);
                wait_for([this] { return field.get_text() == "de"; },
                         "keyboard selection/replacement failed");
                shortcut(4); shortcut(6);
                click(40, 75); shortcut(4); shortcut(25);
                wait_for([this] { return multi.get_text() == "de"; },
                         "menu stole focused editor Ctrl+C/Ctrl+V");
                wait_for([this] {
                    native::modal_wnd modal(*this, "Modal", 180, 90, 180, 100);
                    modal.create(); modal.show(); modal.destroy();
                    native::modeless_wnd modeless(*this, "Modeless", 180, 90, 180, 100);
                    modeless.create(); modeless.show(); modeless.destroy();
                    return true;
                }, "owned windows failed");
                click(40, 30); shortcut(4); key(9);
                wait_for([this] { return field.get_text() == "f"; },
                         "field lost input after modal/modeless close");
                auto done = std::make_shared<std::promise<void>>();
                auto future = done->get_future();
                native::app::post([this, done] {
                    native::open_file_dialog selector(*this);
                    selector.set_initial_path("/tmp");
                    selector.create(); selector.show();
                    done->set_value();
                });
                std::this_thread::sleep_for(150ms);
                key(41);
                expect(future.wait_for(3s) == std::future_status::ready,
                       "file selector did not cancel");
                click(40, 30); shortcut(4); key(10);
                wait_for([this] { return field.get_text() == "g"; },
                         "field lost input after file selector close");
                click(50, 173); click(200, 173);
                wait_for([this] { return multi.get_text() == "g"; },
                         "clipboard buttons failed after dialogs");
                check_collections();
                check_inspector();
                click(40, 30); shortcut(4); key(11);
                wait_for([this] { return field.get_text() == "h"; },
                         "field lost input after collection scrollbar capture");
                std::cout << "real-loop editing, clipboard and collection input passed\n";
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n';
                failures = 1;
            }
            native::app::post([this] { destroy(); });
        }
    };
}

int program(int, char **) {
    input_window window;
    const int result = native::app::run(window);
    return result ? result : window.failures;
}
