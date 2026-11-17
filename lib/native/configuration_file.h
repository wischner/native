//
// Shares bounded settings-file loading and atomic key merging between the
// XDG text adapter and Haiku's flattened BMessage adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include "configuration_backend.h"
#include <filesystem>
#include <map>
#include <span>
#include <vector>

namespace native::detail
{
    using configuration_entries = std::map<std::string, configuration_value>;

    class configuration_file_backend : public configuration_backend
    {
    public:
        // Capture the user file and optional ordered system-default files.
        explicit configuration_file_backend(std::filesystem::path path,
            std::vector<std::filesystem::path> defaults = {});

        // Read the first matching user/system value without creating files.
        std::optional<configuration_value> read(const std::string &key) override;

        // Lock, reload, merge and atomically replace the user file.
        void write(const std::string &key,
            const std::optional<configuration_value> &value) override;

    protected:
        // Decode a complete, bounded file; malformed data must throw.
        virtual configuration_entries decode(std::span<const char> bytes) = 0;
        // Encode all entries using the platform's native file representation.
        virtual std::vector<char> encode(const configuration_entries &entries) = 0;

    private:
        configuration_entries load(const std::filesystem::path &path);
        std::filesystem::path _path;
        std::vector<std::filesystem::path> _defaults;
    };
}
