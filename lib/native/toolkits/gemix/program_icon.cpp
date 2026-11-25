//
// Publishes the main window's icon as the GEMix desktop program icon.
// GEMix stores resource structures in host order; its x86_64 layout
// has 16-bit WORD fields and 64-bit LONG offsets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "program_icon.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

#include <native/app.h>

#include "../../window_icon.h"

namespace
{
    namespace fs = std::filesystem;

    constexpr int edge = 32;
    constexpr std::size_t plane = edge / 8 * edge;
    // RSHDR (36 bytes) is padded so ICONBLK starts on an 8-byte LONG.
    constexpr std::size_t iconblk = 40;
    constexpr std::size_t iconblk_size = 48;
    constexpr std::size_t free_strings = iconblk + iconblk_size;
    constexpr std::size_t mask_offset = free_strings + 8;
    constexpr std::size_t data_offset = mask_offset + plane;
    constexpr std::size_t text_offset = data_offset + plane;

    // Store a little-endian value of the given width.
    void put(std::vector<std::uint8_t> &bytes, std::size_t offset,
             std::uint64_t value, std::size_t width) {
        for (std::size_t i = 0; i < width; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }

    // Set one pixel in a plane of 16-bit words, leftmost pixel highest.
    void set_bit(std::vector<std::uint8_t> &bytes, std::size_t offset,
                 int x, int y) {
        const std::size_t word =
            offset + (std::size_t(y) * (edge / 16) + x / 16) * 2;
        const unsigned bit = 15u - unsigned(x % 16);
        bytes[word + bit / 8] |= static_cast<std::uint8_t>(1u << (bit % 8));
    }

    // Return an absolute environment directory, or an empty path.
    fs::path environment_directory(const char *name) {
        const char *value = std::getenv(name);
        const fs::path path = value ? fs::path(value) : fs::path();
        return path.is_absolute() ? path : fs::path();
    }

    // Name the running program the way the desktop names its entry.
    std::string program_stem() {
        std::error_code error;
        fs::path program = fs::read_symlink("/proc/self/exe", error);
        if (error && native::app::argv && native::app::argv[0])
            program = native::app::argv[0];
        return program.stem().string();
    }
} // namespace

namespace native::detail
{
    std::vector<std::uint8_t> encode_gem_program_icon(
        const img &icon, std::string_view label) {
        const auto square = fit_window_icon(icon, edge);
        std::vector<std::uint8_t> bytes(text_offset + label.size() + 1, 0);

        // RSHDR: one icon block and one free string; empty trees.
        const std::uint16_t header[18] = {
            0, 36, 36, iconblk, free_strings, free_strings, free_strings,
            mask_offset, mask_offset, mask_offset,
            0, 0, 0, 1, 0, 1, 0,
            static_cast<std::uint16_t>(bytes.size())};
        for (std::size_t i = 0; i < 18; ++i)
            put(bytes, i * 2, header[i], 2);

        // ICONBLK: bitmap and label offsets, then a 32x32 icon with
        // its label area directly below the bitmap.
        put(bytes, iconblk, mask_offset, 8);
        put(bytes, iconblk + 8, data_offset, 8);
        put(bytes, iconblk + 16, text_offset, 8);
        put(bytes, iconblk + 34, edge, 2);
        put(bytes, iconblk + 36, edge, 2);
        put(bytes, iconblk + 40, edge, 2);
        put(bytes, iconblk + 42, edge, 2);
        put(bytes, iconblk + 44, 8, 2);
        put(bytes, free_strings, text_offset, 8);

        for (int y = 0; y < edge; ++y)
            for (int x = 0; x < edge; ++x) {
                const rgba &pixel = square->pixels()[y * edge + x];
                if (pixel.a < 32)
                    continue;
                set_bit(bytes, mask_offset, x, y);
                if (std::min({pixel.r, pixel.g, pixel.b}) < 224)
                    set_bit(bytes, data_offset, x, y);
            }
        std::copy(label.begin(), label.end(),
                  bytes.begin() + std::ptrdiff_t(text_offset));
        return bytes;
    }

    fs::path gem_program_icon_directory() {
        const char *override = std::getenv("GEM_RESOURCE_DIR");
        if (override && *override)
            return fs::path(override) / "icons";
        fs::path data = environment_directory("XDG_DATA_HOME");
        if (data.empty()) {
            const fs::path home = environment_directory("HOME");
            if (home.empty())
                return {};
            data = home / ".local" / "share";
        }
        return data / "gem" / "icons";
    }

    void publish_gem_program_icon(const img &icon) noexcept {
        try {
            const std::string stem = program_stem();
            const fs::path directory = gem_program_icon_directory();
            // The desktop gives hidden and overlong names no icon.
            if (stem.empty() || stem.front() == '.' || stem.size() >= 64 ||
                directory.empty())
                return;
            const fs::path target = directory / (stem + ".rsc");
            std::error_code error;
            if (fs::exists(target, error) || error)
                return;
            fs::create_directories(directory, error);
            if (error)
                return;

            std::string label = stem.substr(0, 12);
            std::transform(label.begin(), label.end(), label.begin(),
                           [](unsigned char c) {
                               return char(std::toupper(c));
                           });
            const auto bytes = encode_gem_program_icon(icon, label);
            // Publish complete files only; the desktop may read at once.
            const fs::path partial = directory / (stem + ".rsc.partial");
            {
                std::ofstream file(partial, std::ios::binary);
                file.write(reinterpret_cast<const char *>(bytes.data()),
                           std::streamsize(bytes.size()));
                if (!file.flush()) {
                    file.close();
                    fs::remove(partial, error);
                    return;
                }
            }
            fs::rename(partial, target, error);
            if (error)
                fs::remove(partial, error);
        } catch (...) {
            // Program icons are optional desktop decoration.
        }
    }
} // namespace native::detail
