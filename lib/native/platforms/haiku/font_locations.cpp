//
// Supplies platform font roots without exposing native types to the core.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../font_locations.h"
#include <cstdlib>

namespace native::detail
{
    std::vector<std::filesystem::path> font_roots() {
        std::vector<std::filesystem::path> result = {
            "/boot/system/data/fonts",
            "/boot/system/non-packaged/data/fonts"};
        const char *home = std::getenv("HOME");
        if (home && *home) {
            result.emplace_back(std::filesystem::path(home) /
                                "config/data/fonts");
            result.emplace_back(std::filesystem::path(home) /
                                "config/non-packaged/data/fonts");
        }
        return result;
    }
    std::vector<std::filesystem::path> registered_font_files() {
        return {};
    }
} // namespace native::detail
