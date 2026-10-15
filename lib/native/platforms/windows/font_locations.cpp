//
// Isolates Windows registry font discovery behind a private platform adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../font_locations.h"
#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <string>

namespace
{
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
    std::wstring expand_registry_value(const std::wstring &value) {
        const DWORD required =
            ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (required == 0)
            return value;
        std::vector<wchar_t> expanded(required);
        const DWORD written = ExpandEnvironmentStringsW(
            value.c_str(), expanded.data(), required);
        if (written == 0 || written > required)
            return value;
        return std::wstring(expanded.data());
    }

    void
    append_registry_font_files(std::vector<std::filesystem::path> &files,
                               HKEY root, const wchar_t *key_name,
                               const std::filesystem::path &font_directory) {
        HKEY handle = nullptr;
        if (RegOpenKeyExW(root, key_name, 0, KEY_QUERY_VALUE, &handle) !=
            ERROR_SUCCESS) {
            return;
        }
        registry_key key(handle);

        DWORD value_count = 0;
        DWORD maximum_name_length = 0;
        DWORD maximum_data_length = 0;
        if (RegQueryInfoKeyW(key.get(), nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, &value_count,
                             &maximum_name_length, &maximum_data_length,
                             nullptr, nullptr) != ERROR_SUCCESS) {
            return;
        }

        std::vector<wchar_t> name(maximum_name_length + 1);
        std::vector<wchar_t> data(maximum_data_length / sizeof(wchar_t) + 2);
        for (DWORD index = 0; index < value_count; ++index) {
            DWORD name_length = static_cast<DWORD>(name.size());
            DWORD data_length =
                static_cast<DWORD>(data.size() * sizeof(wchar_t));
            DWORD type = 0;
            const LSTATUS status = RegEnumValueW(
                key.get(), index, name.data(), &name_length, nullptr, &type,
                reinterpret_cast<BYTE *>(data.data()), &data_length);
            if (status != ERROR_SUCCESS ||
                (type != REG_SZ && type != REG_EXPAND_SZ)) {
                continue;
            }

            std::size_t character_count = data_length / sizeof(wchar_t);
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

    std::vector<std::filesystem::path>
    registry_font_files(const std::filesystem::path &font_directory) {
        constexpr const wchar_t *key_names[] = {
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts"};
        const HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
        std::vector<std::filesystem::path> result;
        for (HKEY root : roots) {
            for (const wchar_t *key_name : key_names) {
                append_registry_font_files(result, root, key_name,
                                           font_directory);
            }
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
} // namespace
namespace native::detail
{
    std::vector<std::filesystem::path> font_roots() {
        const char *directory = std::getenv("WINDIR");
        return {directory && *directory
                    ? std::filesystem::path(directory) / "Fonts"
                    : std::filesystem::path("C:/Windows/Fonts")};
    }
    std::vector<std::filesystem::path> registered_font_files() {
        return registry_font_files(font_roots().front());
    }
} // namespace native::detail
