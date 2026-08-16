//
// Enumerates installed TrueType and OpenType files into portable font
// descriptions without exposing platform handles or toolkit values.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/font.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include "portable_font.h"

namespace
{
    constexpr std::uintmax_t maximum_font_file_size =
        128U * 1024U * 1024U;

#if !defined(_WIN32)
    void add_home_path(
        std::vector<std::filesystem::path> &paths,
        const char *suffix) {
        const char *home = std::getenv("HOME");
        if (home && *home)
            paths.emplace_back(std::filesystem::path(home) / suffix);
    }
#endif

    std::vector<std::filesystem::path> font_roots() {
        std::vector<std::filesystem::path> result;
#if defined(_WIN32)
        const char *windows_directory = std::getenv("WINDIR");
        result.emplace_back(
            windows_directory && *windows_directory
                ? std::filesystem::path(windows_directory) / "Fonts"
                : std::filesystem::path("C:/Windows/Fonts"));
#elif defined(__APPLE__)
        result.emplace_back("/System/Library/Fonts");
        result.emplace_back("/Library/Fonts");
        add_home_path(result, "Library/Fonts");
#elif defined(__HAIKU__)
        result.emplace_back("/boot/system/data/fonts");
        result.emplace_back("/boot/system/non-packaged/data/fonts");
        add_home_path(result, "config/data/fonts");
        add_home_path(result, "config/non-packaged/data/fonts");
#else
        result.emplace_back("/usr/share/fonts");
        result.emplace_back("/usr/local/share/fonts");
        add_home_path(result, ".fonts");
        add_home_path(result, ".local/share/fonts");
#endif
        return result;
    }

    bool supported_extension(const std::filesystem::path &path) {
        std::string extension = path.extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return extension == ".ttf" || extension == ".ttc" ||
            extension == ".otf" || extension == ".otc";
    }

    std::vector<std::uint8_t> read_font(
        const std::filesystem::path &path) {
        std::error_code error;
        const std::uintmax_t size =
            std::filesystem::file_size(path, error);
        if (error || size == 0 || size > maximum_font_file_size)
            return {};
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return {};
        std::vector<std::uint8_t> result(
            static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char *>(result.data()),
                    static_cast<std::streamsize>(size));
        if (!stream)
            return {};
        return result;
    }
} // namespace

namespace native
{
    std::vector<font_description> font_t::enumerate_installed() {
        std::vector<font_description> result;
        const auto options =
            std::filesystem::directory_options::skip_permission_denied;
        for (const auto &root : font_roots()) {
            std::error_code error;
            if (!std::filesystem::is_directory(root, error))
                continue;
            std::filesystem::recursive_directory_iterator iterator(
                root, options, error);
            const std::filesystem::recursive_directory_iterator end;
            while (!error && iterator != end) {
                const auto path = iterator->path();
                if (iterator->is_regular_file(error) && !error &&
                    supported_extension(path)) {
                    const auto bytes = read_font(path);
                    auto descriptions = detail::describe_font_data(
                        bytes, path.string());
                    result.insert(
                        result.end(),
                        std::make_move_iterator(descriptions.begin()),
                        std::make_move_iterator(descriptions.end()));
                }
                iterator.increment(error);
                if (error)
                    error.clear();
            }
        }
        std::sort(
            result.begin(),
            result.end(),
            [](const font_description &left,
               const font_description &right) {
                return std::tie(
                           left.family,
                           left.style,
                           left.path,
                           left.face_index) <
                    std::tie(
                           right.family,
                           right.style,
                           right.path,
                           right.face_index);
            });
        result.erase(
            std::unique(
                result.begin(),
                result.end(),
                [](const font_description &left,
                   const font_description &right) {
                    return left.path == right.path &&
                        left.face_index == right.face_index;
                }),
            result.end());
        return result;
    }
} // namespace native
