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

#if defined(_WIN32)
#include <windows.h>
#endif

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

#if defined(_WIN32)
    class registry_key
    {
    public:
        explicit registry_key(HKEY handle) : _handle(handle) {}

        ~registry_key() {
            if (_handle)
                RegCloseKey(_handle);
        }

        registry_key(const registry_key &) = delete;
        registry_key &operator=(const registry_key &) = delete;

        HKEY get() const {
            return _handle;
        }

    private:
        HKEY _handle;
    };
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

#if defined(_WIN32)
    std::wstring expand_registry_value(const std::wstring &value) {
        const DWORD required = ExpandEnvironmentStringsW(
            value.c_str(), nullptr, 0);
        if (required == 0)
            return value;
        std::vector<wchar_t> expanded(required);
        const DWORD written = ExpandEnvironmentStringsW(
            value.c_str(), expanded.data(), required);
        if (written == 0 || written > required)
            return value;
        return std::wstring(expanded.data());
    }

    void append_registry_font_files(
        std::vector<std::filesystem::path> &files,
        HKEY root,
        const wchar_t *key_name,
        const std::filesystem::path &font_directory) {
        HKEY handle = nullptr;
        if (RegOpenKeyExW(
                root, key_name, 0, KEY_QUERY_VALUE, &handle) !=
            ERROR_SUCCESS) {
            return;
        }
        registry_key key(handle);

        DWORD value_count = 0;
        DWORD maximum_name_length = 0;
        DWORD maximum_data_length = 0;
        if (RegQueryInfoKeyW(
                key.get(),
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &value_count,
                &maximum_name_length,
                &maximum_data_length,
                nullptr,
                nullptr) != ERROR_SUCCESS) {
            return;
        }

        std::vector<wchar_t> name(maximum_name_length + 1);
        std::vector<wchar_t> data(
            maximum_data_length / sizeof(wchar_t) + 2);
        for (DWORD index = 0; index < value_count; ++index) {
            DWORD name_length = static_cast<DWORD>(name.size());
            DWORD data_length = static_cast<DWORD>(
                data.size() * sizeof(wchar_t));
            DWORD type = 0;
            const LSTATUS status = RegEnumValueW(
                key.get(),
                index,
                name.data(),
                &name_length,
                nullptr,
                &type,
                reinterpret_cast<BYTE *>(data.data()),
                &data_length);
            if (status != ERROR_SUCCESS ||
                (type != REG_SZ && type != REG_EXPAND_SZ)) {
                continue;
            }

            std::size_t character_count =
                data_length / sizeof(wchar_t);
            while (character_count > 0 &&
                   data[character_count - 1] == L'\0') {
                --character_count;
            }
            if (character_count == 0)
                continue;
            std::wstring value(data.data(), character_count);
            if (type == REG_EXPAND_SZ)
                value = expand_registry_value(value);
            std::filesystem::path path(value);
            if (path.is_relative())
                path = font_directory / path;
            files.push_back(std::move(path));
        }
    }

    std::vector<std::filesystem::path> registry_font_files(
        const std::filesystem::path &font_directory) {
        constexpr const wchar_t *key_names[] = {
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts"};
        const HKEY roots[] = {HKEY_LOCAL_MACHINE,
                              HKEY_CURRENT_USER};
        std::vector<std::filesystem::path> result;
        for (HKEY root : roots) {
            for (const wchar_t *key_name : key_names) {
                append_registry_font_files(
                    result, root, key_name, font_directory);
            }
        }
        std::sort(result.begin(), result.end());
        result.erase(
            std::unique(result.begin(), result.end()), result.end());
        return result;
    }
#endif

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

    std::string utf8_path(const std::filesystem::path &path) {
        const std::u8string encoded = path.u8string();
        return std::string(
            reinterpret_cast<const char *>(encoded.data()),
            encoded.size());
    }

    void append_font_descriptions(
        std::vector<native::font_description> &descriptions,
        const std::filesystem::path &path) {
        const auto bytes = read_font(path);
        auto additions = native::detail::describe_font_data(
            bytes, utf8_path(path));
        descriptions.insert(
            descriptions.end(),
            std::make_move_iterator(additions.begin()),
            std::make_move_iterator(additions.end()));
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
                    append_font_descriptions(result, path);
                }
                iterator.increment(error);
                if (error)
                    error.clear();
            }
        }
#if defined(_WIN32)
        const auto roots = font_roots();
        const std::filesystem::path font_directory = roots.front();
        for (const auto &path :
             registry_font_files(font_directory)) {
            std::error_code error;
            if (std::filesystem::is_regular_file(path, error) &&
                !error && supported_extension(path)) {
                append_font_descriptions(result, path);
            }
        }
#endif
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
