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
        std::vector<std::filesystem::path> result = {"/System/Library/Fonts",
                                                     "/Library/Fonts"};
        const char *home = std::getenv("HOME");
        if (home && *home) {
            result.emplace_back(std::filesystem::path(home) /
                                "Library/Fonts");
        }
        return result;
    }
    std::vector<std::filesystem::path> registered_font_files() {
        return {};
    }
} // namespace native::detail
