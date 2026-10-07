//
// Implements a line-oriented command terminal for every Native backend.
// External GPL-attributed CRT packages process an ANSI screen raster;
// shell commands use bounded public process capture, not a
// pseudoterminal session.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_retro_terminal.h"
#include "terminal_screen.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>

namespace
{
    class retro_terminal : public native::app_wnd
    {
    public:
        retro_terminal(const std::filesystem::path &assets, bool smoke)
            : app_wnd("Native CRT shader monitor", 80, 80, 800, 636),
              _smoke(smoke), _screen(80, 25), _font(native::font_t::from_file(
                                 assets / "terminal.ttf", 8)),
              _effect(native::shader_package::load(assets /
                                                   "terminal-glsl.nshader")),
              _run("Run"), _cancel("Cancel") {
            if (!_font.valid())
                throw std::runtime_error(
                    "Cannot load the portable terminal font.");
            _entry.set_validator([](const std::string &text) {
                return text.size() <= 4096;
            });
            _view.set_fit(native::shader_fit::stretch);
            _view.set_parent(this);
            _entry.set_parent(this);
            _run.set_parent(this);
            _cancel.set_parent(this);
            auto layout =
                std::make_unique<native::grid_layout_manager>();
            (*layout) << native::row(native::star())
                      << native::row(native::pixels(36))
                      << native::column(native::star())
                      << native::column(native::pixels(80))
                      << native::column(native::pixels(80))
                      << native::cell(_view, 0, 0, 1, 3)
                      << native::cell(_entry, 1, 0)
                      << native::cell(_run, 1, 1)
                      << native::cell(_cancel, 1, 2);
            set_layout(std::move(layout));
            native::menu_items_proxy appearance;
            appearance
                << std::pair<int, std::string>{1, "&Amber"}
                << std::pair<int, std::string>{2, "&Green"}
                << std::pair<int, std::string>{3, "&Color"}
                << std::pair<int, std::string>{4, "&Original pixels"}
                << std::pair<int, std::string>{5, "Toggle &animation"}
                << std::pair<int, std::string>{6,
                                               "Toggle cursor &hiding"}
                << std::pair<int, std::string>{7, "&Clear"};
            menu << "&Display" << appearance;
            on_wnd_create.connect([this] {
                create_children();
                return false;
            });
            _run.on_click.connect([this] {
                execute();
                return true;
            });
            _cancel.on_click.connect([this] {
                _worker.request_stop();
                return true;
            });
            _entry.on_key.connect([this](native::key_event event) {
                if (event.action == native::key_action::press &&
                    event.key == native::key_code::enter) {
                    execute();
                    return true;
                }
                return false;
            });
            on_menu.connect([this](int command) {
                appearance_command(command);
                return true;
            });
            _view.on_shader_error.connect(
                [this](const std::string &error) {
                    _failed = true;
                    std::cerr << "Original GLSL renderer: " << error << '\n';
                    set_title(error);
                    if (_smoke)
                        finish_smoke();
                    return false;
                });
            _view.on_wnd_paint.connect([this](native::wnd_paint_event) {
                if (_smoke && _smoke_done &&
                    _view.get_shader_status() ==
                        native::shader_status::active)
                    finish_smoke();
                return false;
            });
        }
        ~retro_terminal() override {
            stop();
        }
        bool failed() const {
            return _failed;
        }

    protected:
        void on_native_destroy() override {
            stop();
            native::app_wnd::on_native_destroy();
        }

    private:
        // Close delivery before stopping and joining the command
        // worker.
        void stop() {
            if (_delivery)
                _delivery->close();
            _worker.request_stop();
            if (_worker.joinable())
                _worker.join();
        }
        // Defer closure until the current paint or delivery callback
        // unwinds.
        void finish_smoke() {
            if (_finishing || !_delivery)
                return;
            _finishing = true;
            _delivery->sender().post([](native::wnd &window) {
                static_cast<retro_terminal &>(window).request_close();
            });
        }
        // Create native controls, load the effect and start optional
        // acceptance.
        void create_children() {
            _delivery.emplace(*this);
            _view.create();
            _view.show();
            _entry.create();
            _entry.show();
            _run.create();
            _run.show();
            _cancel.create();
            _cancel.show();
            _screen.feed(
                "\x1b[1;97mNATIVE CRT SHADER MONITOR\x1b[0m\r\n"
                "Original cool-retro-term GLSL renderer\r\n"
                "Original cool-retro-term GLSL / OpenGL image passes.\r\n"
                "Display: amber / green / color / original "
                "pixels.\r\n\r\n");
            colors();
            _screen.feed(
                "\x1b[9;3H\x1b[36mSIGNAL / PHOSPHOR / GLASS\x1b[0m"
                "\x1b[11;3H  __      __              __      __"
                "\x1b[12;3H_/  \x5c____/  \x5c____________/  \x5c____/ "
                " \x5c__"
                "\x1b[14;3H\x1b[33mMULTIPASS IMAGE EFFECT  /  NATIVE "
                "GLSL PROFILE"
                "\x1b[16;3H\x1b[37mCurvature  Scanlines  Bloom  "
                "Persistence"
                "\x1b[17;3HGlass      Vignette   Frame  Noise / Flicker"
                "\x1b[19;1H\x1b[0m");
            std::string error;
            if (!_view.set_effect(_effect, error))
                throw std::runtime_error(error);
            _view.set_fallback(native::shader_fallback::require_effect);
            if (_smoke) {
                _view.set_cursor(native::mouse_cursor::hidden);
                _screen.feed("\x1b[32mANSI/CRT smoke: font, cells, "
                             "palette, cursor and passes.\x1b[0m\r\n");
                _screen.feed(
                    "\x1b[18;1H\x1b[44;97m  CURSOR ADDRESSING / "
                    "BACKGROUND COLOR  \x1b[0m");
            }
            redraw();
            if (_smoke)
                start_command("echo NATIVE_TERMINAL_COMMAND_OK");
        }
        // Feed a sixteen-color ANSI fixture through the same screen
        // parser.
        void colors() {
            for (unsigned i = 0; i < 16; ++i)
                _screen.feed(
                    "\x1b[" +
                    std::to_string(i < 8 ? 30 + i : 90 + i - 8) + "m" +
                    std::string(3, '#'));
            _screen.feed("\x1b[0m\r\n\r\n");
        }
        // Publish a new immutable source raster and its monotonic
        // timestamp.
        void redraw() {
            auto image = _screen.render(_font, 32, 16);
            const double time =
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - _started)
                    .count();
            _view.set_source(*image, ++_sequence, time);
        }
        // Apply display menu choices through the public shader and
        // cursor API.
        void appearance_command(int command) {
            if (command >= 1 && command <= 3) {
                std::string error;
                if (!_view.get_effect().get_valid() &&
                    !_view.set_effect(_effect, error)) {
                    set_title(error);
                    return;
                }
                const std::array<float, 4> tint =
                    command == 1
                        ? std::array<float, 4>{1, 0.65f, 0.2f, 1}
                    : command == 2
                        ? std::array<float, 4>{0.2f, 1, 0.25f, 1}
                        : std::array<float, 4>{1, 1, 1, 1};
                _view.set_parameter("tint", tint);
            } else if (command == 4)
                _view.clear_effect();
            else if (command == 5)
                _view.set_animation(
                    _view.get_animation() ==
                            native::shader_animation::continuous
                        ? native::shader_animation::source_updates
                        : native::shader_animation::continuous);
            else if (command == 6)
                _view.set_cursor(_view.get_cursor() ==
                                         native::mouse_cursor::hidden
                                     ? native::mouse_cursor::arrow
                                     : native::mouse_cursor::hidden);
            else if (command == 7) {
                _screen.clear();
                _view.clear_history();
                redraw();
            }
        }
        // Handle built-in commands or start one bounded host-shell
        // command.
        void execute() {
            if (_running)
                return;
            std::string command = _entry.get_text();
            _entry.set_text("");
            if (command.empty())
                return;
            _screen.feed("\x1b[0m> " + command + "\r\n");
            if (command == "help") {
                _screen.feed(
                    "help / colors / clear / echo TEXT\r\n"
                    "Other lines run through the host command "
                    "shell.\r\n"
                    "Captured output is bounded; stdin is "
                    "disconnected.\r\n"
                    "Use Run on GEMix; Enter uses physical-key "
                    "support.\r\n");
            } else if (command == "clear") {
                _screen.clear();
                _view.clear_history();
            } else if (command == "colors")
                colors();
            else if (command.starts_with("echo "))
                _screen.feed(command.substr(5) + "\r\n");
            else {
                start_command(std::move(command));
            }
            redraw();
        }
        // Capture a noninteractive shell on a worker and deliver
        // cumulative snapshots.
        void start_command(std::string command) {
            native::process_config config;
#ifdef _WIN32
            const char *system_root = std::getenv("SYSTEMROOT");
            config.executable =
                std::filesystem::path(system_root && *system_root
                                          ? system_root
                                          : "C:\\Windows") /
                "System32/WindowsPowerShell/v1.0/powershell.exe";
            config.arguments = {"-NoLogo", "-NoProfile",
                                "-NonInteractive", "-Command",
                                std::move(command)};
#else
            config.executable = "/bin/sh";
            config.arguments = {"-c", std::move(command)};
#endif
            config.capture_limit = 32 * 1024;
            config.stop_timeout = std::chrono::milliseconds(100);
            _output_size = _error_size = 0;
            _running = true;
            _entry.set_read_only(true);
            const auto sender = _delivery->sender();
            _worker = std::jthread([config = std::move(config),
                                    sender](std::stop_token stop) {
                native::process child(config);
                child.start();
                while (child.get_running()) {
                    if (stop.stop_requested())
                        child.request_stop();
                    auto result = child.get_result();
                    sender.post_latest([result = std::move(result)](
                                           native::wnd &window) {
                        static_cast<retro_terminal &>(window).output(
                            result, false);
                    });
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(50));
                }
                child.wait();
                auto result = child.get_result();
                sender.post_latest(
                    [result = std::move(result)](native::wnd &window) {
                        static_cast<retro_terminal &>(window).output(
                            result, true);
                    });
            });
        }
        // Consume only new captured bytes; coalesced snapshots retain
        // all output.
        void output(const native::process_result &result,
                    bool finished) {
            bool changed = false;
            if (result.output.size() > _output_size) {
                _screen.feed(std::string_view(result.output)
                                 .substr(_output_size));
                _output_size = result.output.size();
                changed = true;
            }
            if (result.error.size() > _error_size) {
                _screen.feed(
                    std::string_view(result.error).substr(_error_size));
                _error_size = result.error.size();
                changed = true;
            }
            if (finished) {
                if (_smoke) {
                    _smoke_done = true;
                    _failed |=
                        result.reason != native::process_exit::exited ||
                        result.exit_code != 0 ||
                        result.output.find(
                            "NATIVE_TERMINAL_COMMAND_OK") ==
                            std::string::npos;
                    if (_failed)
                        std::cerr << "Terminal smoke child: exit="
                                  << result.exit_code
                                  << " output=" << result.output
                                  << " error=" << result.error
                                  << " launch=" << result.launch_error
                                  << '\n';
                }
                _running = false;
                _entry.set_read_only(false);
                _screen.feed("\r\n[exit " +
                             std::to_string(result.exit_code) + "] " +
                             result.launch_error + "\r\n");
                if (result.output_truncated || result.error_truncated)
                    _screen.feed(
                        "[captured output truncated at 32 KiB per "
                        "stream]\r\n");
                changed = true;
            }
            if (changed)
                redraw();
        }
        bool _smoke, _failed = false, _finishing = false,
                     _running = false, _smoke_done = false;
        vision::terminal_screen _screen;
        native::font_t _font;
        native::shader_package _effect;
        native::shader_view _view;
        native::text_edit _entry;
        native::button _run, _cancel;
        std::optional<native::ui_dispatch_scope> _delivery;
        std::jthread _worker;
        std::size_t _output_size = 0, _error_size = 0;
        std::uint64_t _sequence = 0;
        std::chrono::steady_clock::time_point _started =
            std::chrono::steady_clock::now();
    };
} // namespace
namespace vision
{
    int run_retro_terminal(int argc, char **argv) {
        try {
            auto assets = std::filesystem::absolute(
                              std::filesystem::path(argv[0]))
                              .parent_path() /
                          "retro-terminal";
            bool smoke = false;
            for (int i = 1; i < argc; ++i) {
                if (std::string_view(argv[i]) == "--terminal-smoke")
                    smoke = true;
                else if (std::string_view(argv[i]) ==
                             "--terminal-assets" &&
                         i + 1 < argc)
                    assets = argv[++i];
            }
            retro_terminal window(assets, smoke);
            const int result = native::app::run(window);
            return window.failed() ? 1 : result;
        } catch (const std::exception &error) {
            std::cerr << "CRT terminal: " << error.what() << '\n';
            return 1;
        }
    }
} // namespace vision
