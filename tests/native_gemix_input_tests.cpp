//
// Drives real Rasta packets through the application loop, not editor helpers.
// Covers focus, typing and clipboard buttons/shortcuts in both GEM transports.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/gemix/globals.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
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
            paste("Paste text", 160, 160, 120, 28) {
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
                std::cout << "real-loop field, multiline and clipboard input passed\n";
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
