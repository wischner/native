//
// Shares bounded binary file I/O between pack and manifest readers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace native::detail
{
    inline constexpr std::size_t resource_limit = 64 * 1024 * 1024;
    // Read at most limit bytes, rejecting incomplete or growing files.
    std::vector<std::uint8_t> read_resource_file(
        const std::filesystem::path &path, std::size_t limit);
    // Write all bytes and report open, write and close failures.
    void write_resource_file(const std::filesystem::path &path,
                             std::span<const std::uint8_t> bytes);
}
