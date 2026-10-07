//
// Declares shell-free child ownership with bounded output and explicit cancellation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace native
{
    namespace detail { struct process_peer; }
    enum class process_stream { inherit, capture, discard };
    enum class process_exit { not_started, exited, signalled, cancelled, failed };
    struct process_config
    {
        std::filesystem::path executable;
        std::vector<std::string> arguments;
        std::filesystem::path working_directory;
        bool inherit_environment = true;
        // UTF-8 names/values override inherited entries; no shell expansion.
        std::vector<std::pair<std::string, std::string>> environment;
        process_stream output = process_stream::capture;
        process_stream error = process_stream::capture;
        // Captured bytes per stream; overflow is drained and marked truncated.
        std::size_t capture_limit = 1024 * 1024;
        std::chrono::milliseconds stop_timeout{500};
    };
    struct process_result
    {
        process_exit reason = process_exit::not_started;
        int exit_code = 0;
        int signal = 0;
        std::string output, error, launch_error;
        bool output_truncated = false, error_truncated = false;
    };

    // Owns one child, bounded captured streams, and an independent monitor.
    class process
    {
    public:
        // Cache executable/arguments. No resources are created until start().
        explicit process(process_config config);
        // Request cancellation and reap; stop/join on a controller, not UI.
        ~process();
        process(const process &) = delete;
        process &operator=(const process &) = delete;
        // Launch without a shell; false reports launch_error in get_result().
        // A process instance starts once. Repeated start returns false.
        bool start();
        // Return a thread-safe running snapshot.
        bool get_running() const;
        // Request cancellation of this child only; never waits for exit.
        void request_stop();
        // Wait for monitor completion. Never call on the UI for slow children.
        void wait();
        // Return captured result so far; terminal after wait or running=false.
        process_result get_result() const;
    private:
        std::unique_ptr<detail::process_peer> _peer;
    };
}
