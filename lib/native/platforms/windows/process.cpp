//
// Owns Windows child processes with explicit argument quoting and bounded pipe capture.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/process.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "../../process_backend.h"

namespace
{
    struct handle_owner
    {
        HANDLE value = nullptr;
        ~handle_owner() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        HANDLE release() { HANDLE result = value; value = nullptr; return result; }
    };
    // UTF-8 arguments are converted at the OS boundary, rejecting invalid input.
    std::wstring wide(const std::string &text) {
        if (text.find('\0') != std::string::npos) throw std::invalid_argument("Embedded NUL.");
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!length) throw std::invalid_argument("Invalid UTF-8 argument.");
        std::wstring result(length, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), length);
        return result;
    }
    // Quote for the standard Windows C runtime argument parser, without a shell.
    std::wstring quote(const std::wstring &argument) {
        std::wstring result = L"\"";
        unsigned slashes = 0;
        for (wchar_t ch : argument) {
            if (ch == L'\\') { ++slashes; continue; }
            if (ch == L'\"') result.append(slashes * 2 + 1, L'\\');
            else result.append(slashes, L'\\');
            slashes = 0; result += ch;
        }
        result.append(slashes * 2, L'\\'); result += L'\"';
        return result;
    }
    void collect(native::detail::process_peer &peer, HANDLE &pipe, bool error) {
        if (!pipe) return;
        for (unsigned turn = 0; turn < 16; ++turn) {
            DWORD available = 0, count = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
                CloseHandle(pipe); pipe = nullptr; return;
            }
            if (!available) return;
            char bytes[4096];
            if (!ReadFile(pipe, bytes, std::min<DWORD>(available, sizeof(bytes)),
                          &count, nullptr)) { CloseHandle(pipe); pipe = nullptr; return; }
            std::lock_guard guard(peer.mutex);
            auto &text = error ? peer.result.error : peer.result.output;
            auto &truncated = error ? peer.result.error_truncated : peer.result.output_truncated;
            const auto admitted = std::min<std::size_t>(count,
                peer.config.capture_limit - text.size());
            text.append(bytes, admitted); truncated |= admitted != count;
        }
    }
    void monitor(native::detail::process_peer &peer, HANDLE child,
                 HANDLE out, HANDLE err, std::stop_token stop) {
        struct cleanup {
            HANDLE child;
            HANDLE &out, &err;
            ~cleanup() {
                if (WaitForSingleObject(child, 0) == WAIT_TIMEOUT) {
                    TerminateProcess(child, 1);
                    WaitForSingleObject(child, INFINITE);
                }
                if (out) CloseHandle(out);
                if (err) CloseHandle(err);
                CloseHandle(child);
            }
        } resources{child, out, err};
        bool cancelled = false;
        auto deadline = std::chrono::steady_clock::time_point::max();
        DWORD code = 0;
        while (WaitForSingleObject(child, 0) == WAIT_TIMEOUT) {
            collect(peer, out, false); collect(peer, err, true);
            if (stop.stop_requested() && !cancelled) {
                cancelled = true;
                // No universal Windows graceful stop. Allow application protocol
                // time before terminating this explicitly owned child only.
                deadline = std::chrono::steady_clock::now() + peer.config.stop_timeout;
            }
            if (cancelled && std::chrono::steady_clock::now() >= deadline)
                TerminateProcess(child, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        collect(peer, out, false); collect(peer, err, true);
        GetExitCodeProcess(child, &code);
        {
            std::lock_guard guard(peer.mutex);
            peer.result.reason = cancelled ? native::process_exit::cancelled : native::process_exit::exited;
            peer.result.exit_code = static_cast<int>(code);
        }
        peer.running = false;
    }
}
namespace native
{
    bool process::start() {
        if (_peer->started) return false;
        _peer->started = true;
        auto &config = _peer->config;
        const auto fail = [&] {
            std::lock_guard guard(_peer->mutex);
            _peer->result.reason = process_exit::failed;
            _peer->result.launch_error = "Windows error " + std::to_string(GetLastError());
            return false;
        };
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        handle_owner input, output, error, out_read, err_read;
        input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, 0, nullptr);
        const auto stream = [&](process_stream policy, DWORD standard,
                                handle_owner &child, handle_owner &reader) {
            if (policy == process_stream::capture) {
                if (!CreatePipe(&reader.value, &child.value, &security, 0)) return false;
                return SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0) != 0;
            }
            if (policy == process_stream::inherit) {
                HANDLE source = GetStdHandle(standard);
                if (source && source != INVALID_HANDLE_VALUE && DuplicateHandle(
                    GetCurrentProcess(), source, GetCurrentProcess(), &child.value,
                    0, TRUE, DUPLICATE_SAME_ACCESS)) return true;
            }
            child.value = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                &security, OPEN_EXISTING, 0, nullptr);
            return child.value != INVALID_HANDLE_VALUE;
        };
        if (input.value == INVALID_HANDLE_VALUE ||
            !stream(config.output, STD_OUTPUT_HANDLE, output, out_read) ||
            !stream(config.error, STD_ERROR_HANDLE, error, err_read)) return fail();
        std::wstring command = quote(config.executable.wstring());
        for (const auto &argument : config.arguments) command += L" " + quote(wide(argument));
        // Windows environments are case-insensitive, including overrides.
        struct case_less {
            bool operator()(const std::wstring &a, const std::wstring &b) const {
                return _wcsicmp(a.c_str(), b.c_str()) < 0;
            }
        };
        std::map<std::wstring, std::wstring, case_less> variables;
        if (config.inherit_environment) {
            wchar_t *block = GetEnvironmentStringsW();
            if (!block) return fail();
            for (const wchar_t *p = block; *p; p += wcslen(p) + 1) {
                std::wstring entry(p);
                auto split = entry.find(L'=', entry[0] == L'=' ? 1 : 0);
                if (split != std::wstring::npos) variables[entry.substr(0, split)] = entry.substr(split + 1);
            }
            FreeEnvironmentStringsW(block);
        }
        for (const auto &[name, value] : config.environment) {
            if (name.empty() || name.find('=') != std::string::npos)
                throw std::invalid_argument("Invalid environment name.");
            variables[wide(name)] = wide(value);
        }
        std::vector<wchar_t> environment;
        for (const auto &[name, value] : variables) {
            const auto entry = name + L"=" + value;
            environment.insert(environment.end(), entry.begin(), entry.end());
            environment.push_back(0);
        }
        environment.push_back(0);
        if (variables.empty()) environment.push_back(0);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = input.value;
        startup.StartupInfo.hStdOutput = output.value;
        startup.StartupInfo.hStdError = error.value;
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> attributes(bytes);
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes)) return fail();
        HANDLE handles[] = {input.value, output.value, error.value};
        const bool updated = UpdateProcThreadAttribute(startup.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr);
        PROCESS_INFORMATION child{};
        const bool created = updated && CreateProcessW(config.executable.c_str(), command.data(),
            nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
            EXTENDED_STARTUPINFO_PRESENT, environment.data(),
            config.working_directory.empty() ? nullptr : config.working_directory.c_str(),
            &startup.StartupInfo, &child);
        const DWORD failure = GetLastError();
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        if (!created) { SetLastError(failure); return fail(); }
        CloseHandle(child.hThread);
        _peer->running = true;
        HANDLE out = out_read.release(), err = err_read.release();
        try {
            _peer->worker = std::jthread([peer = _peer.get(), child, out, err]
                (std::stop_token) {
                    try { monitor(*peer, child.hProcess, out, err, peer->cancel.get_token()); }
                    catch (...) {
                        std::lock_guard guard(peer->mutex);
                        peer->result.reason = process_exit::failed;
                        peer->running = false;
                    }
                });
        } catch (...) {
            TerminateProcess(child.hProcess, 1);
            WaitForSingleObject(child.hProcess, INFINITE);
            CloseHandle(child.hProcess);
            if (out) CloseHandle(out);
            if (err) CloseHandle(err);
            _peer->running = false;
            throw;
        }
        return true;
    }
}
