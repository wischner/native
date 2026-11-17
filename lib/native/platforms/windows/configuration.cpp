//
// Stores desktop preferences in HKCU\Software\<application>. Dotted key
// prefixes become subkeys. Strings use REG_SZ, booleans REG_DWORD, integers
// REG_QWORD, and doubles an eight-byte REG_BINARY value. No handle escapes
// the adapter; Windows owns registry persistence and synchronization.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../configuration_backend.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
    using namespace native::detail;

    void check(LSTATUS status) {
        if (status != ERROR_SUCCESS)
            throw std::system_error(status, std::system_category(), "Configuration registry access");
    }

    std::wstring wide(const std::string &text) {
        if (text.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!size) check(GetLastError());
        std::wstring result(size, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), size)) check(GetLastError());
        return result;
    }

    std::string utf8(const std::wstring &text) {
        if (text.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (!size) check(GetLastError());
        std::string result(size, '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), size, nullptr, nullptr)) check(GetLastError());
        return result;
    }

    struct registry_key
    {
        HKEY value = nullptr;
        registry_key() = default;
        registry_key(const registry_key &) = delete;
        registry_key &operator=(const registry_key &) = delete;
        ~registry_key() { if (value) RegCloseKey(value); }
    };

    class windows_configuration final : public configuration_backend
    {
    public:
        explicit windows_configuration(const std::string &application)
            : _root(L"Software\\" + wide(application)) {}

        std::optional<configuration_value> read(const std::string &key) override {
            const auto [path, name] = address(key);
            registry_key opened;
            const auto status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(),
                0, KEY_QUERY_VALUE, &opened.value);
            if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return std::nullopt;
            check(status);
            // A bounded single query avoids a query-size/read race with another writer.
            std::vector<BYTE> bytes(2 * configuration_string_limit + sizeof(wchar_t));
            DWORD size = static_cast<DWORD>(bytes.size()), type = 0;
            const auto result = RegQueryValueExW(opened.value, name.c_str(),
                nullptr, &type, bytes.data(), &size);
            if (result == ERROR_FILE_NOT_FOUND) return std::nullopt;
            check(result);
            if (type == REG_SZ && size >= sizeof(wchar_t) && size % sizeof(wchar_t) == 0) {
                std::wstring text(size / sizeof(wchar_t), L'\0');
                std::memcpy(text.data(), bytes.data(), size);
                if (text.back() != L'\0')
                    throw std::runtime_error("Registry configuration string is not terminated.");
                text.pop_back();
                return utf8(text);
            }
            if (type == REG_DWORD && size == sizeof(DWORD)) {
                DWORD value = 0;
                std::memcpy(&value, bytes.data(), sizeof(value));
                if (value > 1) throw std::runtime_error("Invalid registry configuration boolean.");
                return value != 0;
            }
            if (type == REG_QWORD && size == sizeof(std::int64_t)) {
                std::int64_t value = 0;
                std::memcpy(&value, bytes.data(), sizeof(value));
                return value;
            }
            if (type == REG_BINARY && size == sizeof(double)) {
                double value = 0;
                std::memcpy(&value, bytes.data(), sizeof(value));
                return value;
            }
            throw std::runtime_error("Unsupported registry configuration value type.");
        }

        void write(const std::string &key,
            const std::optional<configuration_value> &value) override {
            const auto [path, name] = address(key);
            registry_key opened;
            if (!value) {
                const auto status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(),
                    0, KEY_SET_VALUE, &opened.value);
                if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return;
                check(status);
                const auto result = RegDeleteValueW(opened.value, name.c_str());
                if (result != ERROR_FILE_NOT_FOUND) check(result);
                return;
            }
            check(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &opened.value, nullptr));
            const auto set = [&](DWORD type, const void *data, DWORD size) {
                check(RegSetValueExW(opened.value, name.c_str(), 0, type,
                    static_cast<const BYTE *>(data), size));
            };
            if (const auto *text = std::get_if<std::string>(&*value)) {
                const auto encoded = wide(*text);
                set(REG_SZ, encoded.c_str(), static_cast<DWORD>((encoded.size() + 1) * sizeof(wchar_t)));
            } else if (const auto *flag = std::get_if<bool>(&*value)) {
                const DWORD encoded = *flag ? 1 : 0;
                set(REG_DWORD, &encoded, sizeof(encoded));
            } else if (const auto *integer = std::get_if<std::int64_t>(&*value)) {
                set(REG_QWORD, integer, sizeof(*integer));
            } else {
                const double encoded = std::get<double>(*value);
                set(REG_BINARY, &encoded, sizeof(encoded));
            }
        }

    private:
        std::pair<std::wstring, std::wstring> address(const std::string &key) const {
            const auto split = key.rfind('.');
            if (split == std::string::npos) return {_root, wide(key)};
            auto prefix = wide(key.substr(0, split));
            std::replace(prefix.begin(), prefix.end(), L'.', L'\\');
            return {_root + L"\\" + prefix, wide(key.substr(split + 1))};
        }
        std::wstring _root;
    };
}

namespace native::detail
{
    std::shared_ptr<configuration_backend> make_configuration_backend(
        const std::string &application) {
        return std::make_shared<windows_configuration>(application);
    }
}
