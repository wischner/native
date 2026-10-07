//
// Owns POSIX child launch, simultaneous bounded pipe capture, cancellation and reaping.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/process.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char **environ;

#include "../../process_backend.h"

namespace
{
    // RAII pipe ownership; close-on-exec prevents inherited descriptor leaks.
    struct pipe_pair
    {
        int fd[2] = {-1, -1};
        ~pipe_pair() { for (int descriptor : fd) if (descriptor >= 0) close(descriptor); }
        bool open_pipe() {
            if (pipe(fd) != 0) return false;
            return fcntl(fd[0], F_SETFD, FD_CLOEXEC) == 0 &&
                   fcntl(fd[1], F_SETFD, FD_CLOEXEC) == 0;
        }
        int take_read() { int value = fd[0]; fd[0] = -1; return value; }
    };
    void collect(native::detail::process_peer &peer, int &fd, bool error) {
        if (fd < 0) return;
        char bytes[4096];
        // Bound each pass so stderr, exit checks and cancellation get time.
        for (unsigned turn = 0; turn < 16; ++turn) {
            const ssize_t count = read(fd, bytes, sizeof(bytes));
            if (count > 0) {
                std::lock_guard guard(peer.mutex);
                auto &text = error ? peer.result.error : peer.result.output;
                auto &truncated = error ? peer.result.error_truncated
                                        : peer.result.output_truncated;
                const auto admitted = std::min<std::size_t>(count,
                    peer.config.capture_limit - text.size());
                text.append(bytes, admitted);
                truncated |= admitted != static_cast<std::size_t>(count);
            } else if (!count) { close(fd); fd = -1; return; }
            else if (errno == EINTR) continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            else { close(fd); fd = -1; return; }
        }
    }
    void monitor(native::detail::process_peer &peer, pid_t pid,
                 int out, int err, std::stop_token stop) {
        struct cleanup {
            pid_t pid;
            int &out, &err;
            ~cleanup() {
                if (pid > 0) {
                    kill(pid, SIGKILL);
                    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
                }
                if (out >= 0) close(out);
                if (err >= 0) close(err);
            }
        } resources{pid, out, err};
        bool cancelled = false, exited = false, killed = false;
        int status = 0;
        auto deadline = std::chrono::steady_clock::time_point::max();
        while (!exited) {
            collect(peer, out, false);
            collect(peer, err, true);
            const pid_t observed = waitpid(pid, &status, WNOHANG);
            if (observed == pid) { resources.pid = -1; exited = true; break; }
            if (observed < 0 && errno != EINTR) {
                if (errno == ECHILD) resources.pid = -1;
                std::lock_guard guard(peer.mutex);
                peer.result.reason = native::process_exit::failed;
                peer.result.launch_error = std::strerror(errno);
                break;
            }
            if (stop.stop_requested() && !cancelled) {
                cancelled = true;
                kill(pid, SIGTERM);
                deadline = std::chrono::steady_clock::now() + peer.config.stop_timeout;
            }
            if (cancelled && !killed && std::chrono::steady_clock::now() >= deadline) {
                kill(pid, SIGKILL); killed = true;
            }
            pollfd descriptors[2] = {{out, POLLIN, 0}, {err, POLLIN, 0}};
            poll(descriptors, 2, 5);
        }
        // Only this child is owned. Do not wait for inherited pipes in descendants.
        collect(peer, out, false); collect(peer, err, true);
        {
            std::lock_guard guard(peer.mutex);
            if (exited) {
                peer.result.reason = cancelled ? native::process_exit::cancelled :
                    WIFSIGNALED(status) ? native::process_exit::signalled :
                    native::process_exit::exited;
                if (WIFEXITED(status)) peer.result.exit_code = WEXITSTATUS(status);
                if (WIFSIGNALED(status)) peer.result.signal = WTERMSIG(status);
            }
        }
        peer.running = false;
    }
}
namespace native
{
    bool process::start() {
        if (_peer->started) return false;
        _peer->started = true;
        const auto fail = [&](int error) {
            std::lock_guard guard(_peer->mutex);
            _peer->result.reason = process_exit::failed;
            _peer->result.launch_error = std::strerror(error);
            return false;
        };
        auto &config = _peer->config;
        std::map<std::string, std::string> variables;
        if (config.inherit_environment)
            for (char **entry = environ; entry && *entry; ++entry) {
                std::string text(*entry);
                const auto split = text.find('=');
                if (split != std::string::npos)
                    variables[text.substr(0, split)] = text.substr(split + 1);
            }
        for (const auto &[name, value] : config.environment) {
            if (name.empty() || name.find('=') != std::string::npos ||
                name.find('\0') != std::string::npos || value.find('\0') != std::string::npos)
                return fail(EINVAL);
            variables[name] = value;
        }
        std::vector<std::string> environment;
        for (const auto &[name, value] : variables) environment.push_back(name + "=" + value);
        std::vector<char *> env;
        for (auto &entry : environment) env.push_back(entry.data());
        env.push_back(nullptr);
        auto executable = config.executable;
        // Resolve PATH in the parent, never using a shell or allocating after fork.
        if (!executable.has_parent_path()) {
            auto path = variables.find("PATH");
            const std::string search = path == variables.end() ? "/bin:/usr/bin" : path->second;
            std::size_t begin = 0;
            for (;;) {
                const auto end = search.find(':', begin);
                auto candidate = std::filesystem::path(search.substr(begin, end - begin)) / executable;
                if (candidate.is_relative() && !config.working_directory.empty())
                    candidate = config.working_directory / candidate;
                if (access(candidate.c_str(), X_OK) == 0) { executable = candidate; break; }
                if (end == std::string::npos) return fail(ENOENT);
                begin = end + 1;
            }
        }
        executable = std::filesystem::absolute(executable);
        const std::string executable_text = executable.string();
        std::vector<std::string> arguments{config.executable.string()};
        arguments.insert(arguments.end(), config.arguments.begin(), config.arguments.end());
        std::vector<char *> argv;
        for (auto &argument : arguments) {
            if (argument.find('\0') != std::string::npos) return fail(EINVAL);
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);
        const std::string directory = config.working_directory.string();
        if (directory.find('\0') != std::string::npos) return fail(EINVAL);
        pipe_pair out, err, launch;
        if ((config.output == process_stream::capture && !out.open_pipe()) ||
            (config.error == process_stream::capture && !err.open_pipe()) ||
            !launch.open_pipe()) return fail(errno);
        const int null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null_fd < 0) return fail(errno);
        const pid_t pid = fork();
        if (pid == 0) {
            // Only async-signal-safe calls between fork and exec in a UI app.
            int failure = 0;
            if ((!directory.empty() && chdir(directory.c_str()) != 0) ||
                dup2(null_fd, STDIN_FILENO) < 0) failure = errno;
            const int stdout_fd = config.output == process_stream::capture ? out.fd[1] :
                config.output == process_stream::discard ? null_fd : STDOUT_FILENO;
            const int stderr_fd = config.error == process_stream::capture ? err.fd[1] :
                config.error == process_stream::discard ? null_fd : STDERR_FILENO;
            if (!failure && (dup2(stdout_fd, STDOUT_FILENO) < 0 ||
                             dup2(stderr_fd, STDERR_FILENO) < 0)) failure = errno;
            if (!failure) execve(executable_text.c_str(), argv.data(), env.data());
            if (!failure) failure = errno;
            (void)!write(launch.fd[1], &failure, sizeof(failure));
            _exit(127);
        }
        const int fork_error = errno;
        close(null_fd);
        if (pid < 0) return fail(fork_error);
        close(launch.fd[1]); launch.fd[1] = -1;
        int launch_error = 0;
        ssize_t count;
        do { count = read(launch.fd[0], &launch_error, sizeof(launch_error)); }
        while (count < 0 && errno == EINTR);
        if (count != 0) {
            if (count < 0) { launch_error = errno; kill(pid, SIGKILL); }
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
            return fail(launch_error);
        }
        for (auto *pipe : {&out, &err}) {
            if (pipe->fd[1] >= 0) { close(pipe->fd[1]); pipe->fd[1] = -1; }
            if (pipe->fd[0] >= 0) fcntl(pipe->fd[0], F_SETFL, O_NONBLOCK);
        }
        const int output = out.take_read(), error = err.take_read();
        _peer->running = true;
        try {
            _peer->worker = std::jthread([peer = _peer.get(), pid, output, error]
                (std::stop_token) {
                    try { monitor(*peer, pid, output, error, peer->cancel.get_token()); }
                    catch (...) {
                        std::lock_guard guard(peer->mutex);
                        peer->result.reason = process_exit::failed;
                        peer->running = false;
                    }
                });
        } catch (...) {
            kill(pid, SIGKILL);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
            if (output >= 0) close(output);
            if (error >= 0) close(error);
            _peer->running = false;
            throw;
        }
        return true;
    }
}
