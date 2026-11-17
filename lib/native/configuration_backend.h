//
// Defines the private typed platform preference boundary. Backends return
// absence separately from errors and publish one key per write operation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace native::detail
{
    using configuration_value =
        std::variant<std::string, bool, std::int64_t, double>;

    inline constexpr std::size_t configuration_string_limit = 65536;
    inline constexpr std::size_t configuration_file_limit = 1024 * 1024;

    class configuration_backend
    {
    public:
        virtual ~configuration_backend() = default;
        // Read a native value. Only a missing key returns nullopt.
        virtual std::optional<configuration_value> read(
            const std::string &key) = 0;
        // Merge one key, or delete its user override when value is absent.
        virtual void write(const std::string &key,
            const std::optional<configuration_value> &value) = 0;
    };

    // Resolve the application namespace without creating persistent state.
    std::shared_ptr<configuration_backend> make_configuration_backend(
        const std::string &application);

    // Enforce portable, case-stable dotted identifiers and scalar limits.
    void validate_configuration_name(const std::string &name);
    void validate_configuration_value(const configuration_value &value);
}
