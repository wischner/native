//
// Implements backend-independent font handle queries.
// Creation, destruction, moves, and stock fonts remain backend-specific
// because those operations own native toolkit resources.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/font.h>

#include <stdexcept>

namespace
{
    std::string utf8_from(char32_t value) {
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument("font_t: invalid Unicode character");

        std::string result;
        if (value <= 0x7f) {
            result.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
        else if (value <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
        else {
            result.push_back(static_cast<char>(0xf0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
        return result;
    }
}

namespace native
{
    bool font_t::valid() const {
        return _id != 0;
    }

    const font_spec &font_t::spec() const {
        return _spec;
    }

    std::uint32_t font_t::id() const {
        return _id;
    }

    text_metrics font_t::measure_character(char32_t character) const {
        return measure_text(utf8_from(character));
    }
}
