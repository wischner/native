//
// Owns startup arguments and the application's persistent identity without
// depending on a window backend. An explicit name overrides the lazy default
// filename from argv[0]; configuration bindings capture it when constructed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app.h>

#include "text_util.h"

#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
    struct identity_state
    {
        std::mutex mutex;
        std::string name;
    };

    identity_state &identity() {
        static identity_state state;
        return state;
    }

    // Keep the name a single safe component in both files and registry paths.
    void validate_name(const std::string &name) {
        if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
            name.find_first_of("/\\") != std::string::npos ||
            !native::detail::valid_utf8(name))
            throw std::invalid_argument("Application name must be a nonempty UTF-8 filename component.");
        for (const unsigned char c : name) {
            if (c < 32 || c == 127)
                throw std::invalid_argument("Application name cannot contain control characters.");
        }
    }
}

namespace native
{
    int app::argc = 0;
    char **app::argv = nullptr;
    char **app::envp = nullptr;

    void app::set_name(std::string name) {
        validate_name(name);
        auto &state = identity();
        std::lock_guard lock(state.mutex);
        state.name = std::move(name);
    }

    std::string app::get_name() {
        auto &state = identity();
        std::lock_guard lock(state.mutex);
        if (state.name.empty()) {
            if (argc < 1 || !argv || !argv[0] || !argv[0][0])
                throw std::logic_error("Application arguments are unavailable before program().");
            const std::string_view argument(argv[0]);
            const std::filesystem::path path(
                std::u8string(argument.begin(), argument.end()));
            const auto filename = path.filename().u8string();
            std::string name(filename.begin(), filename.end());
            validate_name(name);
            state.name = std::move(name);
        }
        return state.name;
    }
}
