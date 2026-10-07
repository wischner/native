//
// Tests shell-free argument boundaries, stream bounds, failed launch and child cancellation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/process.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#if !defined(_WIN32)
#include <signal.h>
#endif

namespace
{
    void require(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
}
int main(int argc, char **argv) {
    if (argc > 1 && std::string_view(argv[1]) == "child") {
        std::cout << argv[2] << "|" << argv[3] << "|";
        const char *value = std::getenv("NATIVE_PROCESS_TEST");
        std::cout << (value ? value : "missing");
        std::cerr << "separate stderr";
        return 7;
    }
    if (argc > 1 && std::string_view(argv[1]) == "cwd") {
        std::cout << std::filesystem::current_path().string(); return 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "pipes") {
        for (unsigned i = 0; i < 5000; ++i) {
            std::cout << std::string(80, 'o');
            std::cerr << std::string(80, 'e');
        }
        return 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(20)); return 0;
    }
#if !defined(_WIN32)
    if (argc > 1 && std::string_view(argv[1]) == "ignore-stop") {
        signal(SIGTERM, SIG_IGN);
        std::cout << "ready" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(20)); return 0;
    }
#endif
    try {
        native::process_config config;
        config.executable = std::filesystem::absolute(argv[0]);
        config.arguments = {"child", "spaces and \"quote\"", "trailing\\"};
        config.environment = {{"NATIVE_PROCESS_TEST", "override"}};
        native::process child(config);
        require(child.start() && !child.start(), "Single launch.");
        child.wait();
        auto result = child.get_result();
        require(!child.get_running() && result.reason == native::process_exit::exited &&
            result.exit_code == 7, "Exit status.");
        require(result.output == "spaces and \"quote\"|trailing\\|override" &&
            result.error == "separate stderr", "Argument/env/stream boundaries.");
        config.arguments = {"cwd"};
        config.working_directory = std::filesystem::temp_directory_path();
        config.inherit_environment = false;
        native::process directory(config);
        require(directory.start(), "Working-directory launch."); directory.wait();
        require(std::filesystem::equivalent(directory.get_result().output,
            config.working_directory), "Working directory / explicit environment.");
        config.working_directory.clear(); config.inherit_environment = true;
        config.arguments = {"pipes"}; config.capture_limit = 1000;
        native::process pipes(config);
        require(pipes.start(), "Pipe launch."); pipes.wait();
        result = pipes.get_result();
        require(result.exit_code == 0 && result.output.size() == 1000 &&
            result.error.size() == 1000 && result.output_truncated && result.error_truncated,
            "Simultaneous bounded pipe draining.");
        config.arguments = {"sleep"}; config.stop_timeout = std::chrono::milliseconds(20);
        native::process sleeping(config);
        require(sleeping.start(), "Cancellation launch.");
        sleeping.request_stop(); sleeping.wait();
        require(sleeping.get_result().reason == native::process_exit::cancelled,
            "Cancellation/reaping.");
#if !defined(_WIN32)
        config.arguments = {"ignore-stop"};
        native::process stubborn(config);
        require(stubborn.start(), "Escalation launch.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (stubborn.get_result().output != "ready" &&
               std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(stubborn.get_result().output == "ready", "Child cancellation handshake.");
        stubborn.request_stop(); stubborn.wait();
        require(stubborn.get_result().reason == native::process_exit::cancelled &&
            stubborn.get_result().signal == SIGKILL, "Cancellation escalation/reaping.");
#endif
        config.executable = config.executable.parent_path() / "native-no-such-program";
        native::process absent(config);
        require(!absent.start() && !absent.get_result().launch_error.empty(), "Failed launch.");
        std::cout << "Child process contracts passed.\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
