//
// Implements Unicode-scalar table matching without materializing rows.
// Case folding covers ASCII and the common Latin, Greek, and Cyrillic
// one-to-one mappings used by portable desktop data.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "table_search.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
    std::vector<char32_t> decode(const std::string &text) {
        std::vector<char32_t> result;
        for (std::size_t offset = 0; offset < text.size();) {
            const auto first = static_cast<std::uint8_t>(text[offset]);
            char32_t value = 0xfffd;
            std::size_t count = 1;
            if (first < 0x80) {
                value = first;
            } else if ((first & 0xe0U) == 0xc0U &&
                       offset + 1 < text.size()) {
                value = static_cast<char32_t>(first & 0x1fU) << 6U;
                value |= static_cast<std::uint8_t>(text[offset + 1]) &
                         0x3fU;
                count = 2;
            } else if ((first & 0xf0U) == 0xe0U &&
                       offset + 2 < text.size()) {
                value = static_cast<char32_t>(first & 0x0fU) << 12U;
                value |= static_cast<char32_t>(
                             static_cast<std::uint8_t>(text[offset + 1]) &
                             0x3fU)
                         << 6U;
                value |= static_cast<std::uint8_t>(text[offset + 2]) &
                         0x3fU;
                count = 3;
            } else if ((first & 0xf8U) == 0xf0U &&
                       offset + 3 < text.size()) {
                value = static_cast<char32_t>(first & 0x07U) << 18U;
                value |= static_cast<char32_t>(
                             static_cast<std::uint8_t>(text[offset + 1]) &
                             0x3fU)
                         << 12U;
                value |= static_cast<char32_t>(
                             static_cast<std::uint8_t>(text[offset + 2]) &
                             0x3fU)
                         << 6U;
                value |= static_cast<std::uint8_t>(text[offset + 3]) &
                         0x3fU;
                count = 4;
            }
            result.push_back(value);
            offset += count;
        }
        return result;
    }

    char32_t fold(char32_t value) {
        if (value >= U'A' && value <= U'Z')
            return value + 0x20;
        if ((value >= 0x00c0 && value <= 0x00d6) ||
            (value >= 0x00d8 && value <= 0x00de)) {
            return value + 0x20;
        }
        if ((value >= 0x0391 && value <= 0x03a1) ||
            (value >= 0x03a3 && value <= 0x03ab)) {
            return value + 0x20;
        }
        if (value == 0x03c2)
            return 0x03c3;
        if (value >= 0x0410 && value <= 0x042f)
            return value + 0x20;
        if (value >= 0x0400 && value <= 0x040f)
            return value + 0x50;
        return value;
    }

    std::vector<char32_t> comparable(
        const std::string &text,
        native::table_search_case case_mode) {
        std::vector<char32_t> result = decode(text);
        if (case_mode == native::table_search_case::insensitive) {
            std::transform(result.begin(), result.end(), result.begin(),
                           fold);
        }
        return result;
    }
} // namespace

namespace native::detail
{
    bool table_text_matches(const std::string &value,
                            const std::string &query,
                            table_search_match match,
                            table_search_case case_mode) {
        const std::vector<char32_t> haystack =
            comparable(value, case_mode);
        const std::vector<char32_t> needle =
            comparable(query, case_mode);
        if (match == table_search_match::exact)
            return haystack == needle;
        if (match == table_search_match::prefix) {
            return needle.size() <= haystack.size() &&
                   std::equal(needle.begin(), needle.end(),
                              haystack.begin());
        }
        return std::search(haystack.begin(), haystack.end(),
                           needle.begin(), needle.end()) !=
               haystack.end();
    }
} // namespace native::detail
