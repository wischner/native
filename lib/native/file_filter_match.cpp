//
// Implements portable case-insensitive file-dialog wildcard matching.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_filter_match.h"

#include <algorithm>
#include <cctype>

namespace
{
    void fold_ascii(std::string &value) {
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(
                               std::tolower(character));
                       });
    }
} // namespace

namespace native::detail
{
    bool matches_file_pattern(std::string pattern, std::string value) {
        fold_ascii(pattern);
        fold_ascii(value);
        std::size_t pattern_index = 0;
        std::size_t value_index = 0;
        std::size_t star = std::string::npos;
        std::size_t retry = 0;
        while (value_index < value.size()) {
            if (pattern_index < pattern.size() &&
                (pattern[pattern_index] == '?' ||
                 pattern[pattern_index] == value[value_index])) {
                ++pattern_index;
                ++value_index;
            } else if (pattern_index < pattern.size() &&
                       pattern[pattern_index] == '*') {
                star = pattern_index++;
                retry = value_index;
            } else if (star != std::string::npos) {
                pattern_index = star + 1;
                value_index = ++retry;
            } else {
                return false;
            }
        }
        while (pattern_index < pattern.size() &&
               pattern[pattern_index] == '*') {
            ++pattern_index;
        }
        return pattern_index == pattern.size();
    }
} // namespace native::detail
