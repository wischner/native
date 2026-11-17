//
// Stores Haiku preferences as native flattened BMessages under the user's
// settings directory. Serialization uses bounded memory streams; actual
// file access and atomic publication stay in the shared C++ file adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../configuration_file.h"

#include <DataIO.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <stdexcept>

namespace
{
    using namespace native::detail;

    std::filesystem::path settings_path(const std::string &application) {
        BPath root;
        if (find_directory(B_USER_SETTINGS_DIRECTORY, &root, false) != B_OK)
            throw std::runtime_error("Haiku settings directory is unavailable.");
        return std::filesystem::path(root.Path()) / application / "settings";
    }

    void check(status_t status) {
        if (status != B_OK)
            throw std::runtime_error("Invalid Haiku configuration BMessage.");
    }

    class haiku_configuration final : public configuration_file_backend
    {
    public:
        explicit haiku_configuration(const std::string &application)
            : configuration_file_backend(settings_path(application)) {}

    private:
        configuration_entries decode(std::span<const char> bytes) override {
            // BMemoryIO bounds Unflatten's reads, unlike its raw-pointer overload.
            BMemoryIO input(bytes.data(), bytes.size());
            BMessage message;
            check(message.Unflatten(&input));
            if (input.Position() != static_cast<off_t>(bytes.size()))
                throw std::runtime_error("Trailing data in Haiku configuration.");
            configuration_entries result;
            const int32 names = message.CountNames(B_ANY_TYPE);
            if (names > 1024)
                throw std::runtime_error("Too many Haiku configuration keys.");
            for (int32 i = 0; i < names; ++i) {
                char *key = nullptr;
                type_code type = 0;
                int32 count = 0;
                check(message.GetInfo(B_ANY_TYPE, i, &key, &type, &count));
                if (count != 1 || !key)
                    throw std::runtime_error("Configuration keys require one scalar value.");
                switch (type) {
                case B_STRING_TYPE: {
                    const void *data = nullptr;
                    ssize_t size = 0;
                    check(message.FindData(key, type, &data, &size));
                    if (size <= 0 || static_cast<const char *>(data)[size - 1] != '\0')
                        throw std::runtime_error("Invalid Haiku configuration string.");
                    result.emplace(key, std::string(static_cast<const char *>(data), size - 1));
                    break;
                }
                case B_BOOL_TYPE: {
                    bool value = false;
                    check(message.FindBool(key, &value));
                    result.emplace(key, value);
                    break;
                }
                case B_INT64_TYPE: {
                    int64 value = 0;
                    check(message.FindInt64(key, &value));
                    result.emplace(key, static_cast<std::int64_t>(value));
                    break;
                }
                case B_DOUBLE_TYPE: {
                    double value = 0;
                    check(message.FindDouble(key, &value));
                    result.emplace(key, value);
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported Haiku configuration value type.");
                }
            }
            return result;
        }

        std::vector<char> encode(const configuration_entries &entries) override {
            BMessage message;
            for (const auto &[key, value] : entries) {
                if (const auto *text = std::get_if<std::string>(&value))
                    check(message.AddString(key.c_str(), text->c_str()));
                else if (const auto *flag = std::get_if<bool>(&value))
                    check(message.AddBool(key.c_str(), *flag));
                else if (const auto *integer = std::get_if<std::int64_t>(&value))
                    check(message.AddInt64(key.c_str(), *integer));
                else
                    check(message.AddDouble(key.c_str(), std::get<double>(value)));
            }
            const auto size = message.FlattenedSize();
            if (size < 0 || static_cast<std::size_t>(size) > configuration_file_limit)
                throw std::runtime_error("Haiku configuration is too large.");
            std::vector<char> bytes(static_cast<std::size_t>(size));
            check(message.Flatten(bytes.data(), size));
            return bytes;
        }
    };
}

namespace native::detail
{
    std::shared_ptr<configuration_backend> make_configuration_backend(
        const std::string &application) {
        return std::make_shared<haiku_configuration>(application);
    }
}
