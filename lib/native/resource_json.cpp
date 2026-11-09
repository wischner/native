//
// Implements strict manifest JSON with bounded recursion and full Unicode
// string escapes, including surrogate pairs. No external parser is needed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "resource_json.h"
#include <limits>
#include <stdexcept>

namespace
{
    class parser
    {
    public:
        std::string_view text;
        std::size_t position = 0;
        [[noreturn]] void fail() const {
            throw std::invalid_argument("Invalid resource JSON at byte " +
                                        std::to_string(position));
        }
        void space() {
            while (position < text.size() &&
                   (text[position] == ' ' || text[position] == '\n' ||
                    text[position] == '\r' || text[position] == '\t')) ++position;
        }
        bool consume(char c) {
            space();
            if (position == text.size() || text[position] != c) return false;
            ++position;
            return true;
        }
        void require(char c) {
            if (!consume(c)) fail();
        }
        unsigned hex() {
            unsigned value = 0;
            for (int i = 0; i < 4; ++i) {
                if (position == text.size()) fail();
                char c = text[position++];
                unsigned digit;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else fail();
                value = value * 16 + digit;
            }
            return value;
        }
        void utf8(std::string &out, unsigned value) {
            if (value < 0x80) out += static_cast<char>(value);
            else {
                if (value < 0x800) out += static_cast<char>(0xc0 | (value >> 6));
                else {
                    if (value < 0x10000) out += static_cast<char>(0xe0 | (value >> 12));
                    else {
                        out += static_cast<char>(0xf0 | (value >> 18));
                        out += static_cast<char>(0x80 | ((value >> 12) & 63));
                    }
                    out += static_cast<char>(0x80 | ((value >> 6) & 63));
                }
                out += static_cast<char>(0x80 | (value & 63));
            }
        }
        std::string string() {
            require('"');
            std::string out;
            while (position < text.size()) {
                unsigned char c = text[position++];
                if (c == '"') return out;
                if (c < 32) fail();
                if (c == '\\') {
                    if (position == text.size()) fail();
                    c = text[position++];
                    switch (c) {
                    case '"': case '\\': case '/': out += c; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned value = hex();
                        if (value >= 0xd800 && value <= 0xdbff) {
                            if (position + 2 > text.size() ||
                                text.substr(position, 2) != "\\u") fail();
                            position += 2;
                            unsigned low = hex();
                            if (low < 0xdc00 || low > 0xdfff) fail();
                            value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
                        } else if (value >= 0xdc00 && value <= 0xdfff) fail();
                        utf8(out, value);
                        break;
                    }
                    default: fail();
                    }
                } else if (c < 128) out += c;
                else {
                    unsigned count;
                    unsigned value;
                    unsigned minimum;
                    if (c >= 0xc2 && c <= 0xdf) { count = 1; value = c & 31; minimum = 0x80; }
                    else if (c >= 0xe0 && c <= 0xef) { count = 2; value = c & 15; minimum = 0x800; }
                    else if (c >= 0xf0 && c <= 0xf4) { count = 3; value = c & 7; minimum = 0x10000; }
                    else fail();
                    for (unsigned i = 0; i < count; ++i) {
                        if (position == text.size()) fail();
                        unsigned char next = text[position++];
                        if ((next & 0xc0) != 0x80) fail();
                        value = (value << 6) | (next & 63);
                    }
                    if (value < minimum || value > 0x10ffff ||
                        (value >= 0xd800 && value <= 0xdfff)) fail();
                    utf8(out, value);
                }
                if (out.size() > 4096) fail();
            }
            fail();
        }
        native::detail::resource_json parse(unsigned depth = 0) {
            if (depth > 16) fail();
            space();
            if (position == text.size()) fail();
            using json = native::detail::resource_json;
            if (consume('{')) {
                json::object out;
                if (consume('}')) return {out};
                do {
                    auto key = string();
                    require(':');
                    if (!out.emplace(std::move(key), parse(depth + 1)).second) fail();
                } while (consume(','));
                require('}');
                return {std::move(out)};
            }
            if (consume('[')) {
                json::array out;
                if (consume(']')) return {out};
                do {
                    if (out.size() >= 4096) fail();
                    out.push_back(parse(depth + 1));
                } while (consume(','));
                require(']');
                return {std::move(out)};
            }
            if (text[position] == '"') return {string()};
            const auto start = position;
            std::uint32_t value = 0;
            while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
                unsigned digit = text[position++] - '0';
                if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10) fail();
                value = value * 10 + digit;
            }
            if (position == start || (text[start] == '0' && position - start > 1)) fail();
            return {value};
        }
    };
}
namespace native::detail
{
    resource_json parse_resource_json(std::string_view text) {
        parser input{text};
        auto result = input.parse();
        input.space();
        if (input.position != text.size()) input.fail();
        return result;
    }
}
