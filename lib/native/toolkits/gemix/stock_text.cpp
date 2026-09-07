//
// Reads GEM's immutable stock font for offscreen/rotated text and AES
// scrollbar arrows. Window text still uses VDI; both targets use the
// same resource, while arrow glyphs bypass printable-text encoding.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "stock_text.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include "../../text_util.h"
#include "../../software_image.h"

namespace
{
    struct bitmap_font
    {
        std::vector<unsigned char> data;
        unsigned first = 0, last = 0, stride = 0, height = 0;
        std::size_t bitmap = 0, offsets = 0;

        unsigned word(std::size_t at) const {
            return data[at] | (unsigned(data[at + 1]) << 8);
        }

        bitmap_font() {
            const char *root = std::getenv("GEM_RESOURCE_DIR");
            const std::filesystem::path path = std::filesystem::path(
                root && *root ? root : "/opt/gemix/share/gem") /
                "fonts" / "AtariSTHigh.fnt";
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream || stream.tellg() < 88 || stream.tellg() > 1024 * 1024)
                return;
            stream.seekg(0);
            data.assign(std::istreambuf_iterator<char>(stream), {});
            first = word(36); last = word(38);
            stride = word(80); height = word(82);
            if (first > last || last > 255 || !stride || !height) return;
            const std::size_t bitmap_size = std::size_t(stride) * height;
            const std::size_t offset_size = (last - first + 2) * 2;
            // Hosted GEM resources store the bitmap before the offsets.
            for (std::size_t field : {68, 72, 76}) {
                const std::size_t value = word(field) |
                    (std::size_t(word(field + 2)) << 16);
                if (value && value <= data.size() && bitmap_size <= data.size() - value)
                    bitmap = value;
                if (value && value <= data.size() && offset_size <= data.size() - value &&
                    (!bitmap || value > bitmap)) offsets = value;
            }
            if (!bitmap || offsets <= bitmap) { bitmap = 0; return; }
            for (unsigned glyph = 0; glyph <= last - first; ++glyph) {
                const unsigned start = word(offsets + glyph * 2);
                const unsigned end = word(offsets + (glyph + 1) * 2);
                if (start > end || end > stride * 8) { bitmap = 0; return; }
            }
        }
    };

    // Share one immutable font resource between text and theme symbols.
    const bitmap_font &stock_font() {
        static const bitmap_font font;
        return font;
    }
}

namespace linux::gemix
{
    std::string stock_text(const std::string &text) {
        std::string result;
        for (std::size_t index = 0; index < text.size();) {
            const auto next = native::detail::next_utf8(text, index);
            const auto value = static_cast<unsigned char>(text[index]);
            if (next - index == 3 && text.compare(index, 3, "\xe2\x80\xa6") == 0)
                result += "...";
            else result += next - index == 1 && value >= 32 && value < 127
                ? char(value) : '?';
            index = next > index ? next : index + 1;
        }
        return result;
    }

    bool draw_stock_text(const native::img &image, const native::rect &clip,
                         const std::string &text, native::point position,
                         native::rgba color) {
        const auto &font = stock_font();
        if (!font.bitmap) return false;
        const auto bounds = clip.intersect(native::rect(0, 0, image.w(), image.h()));
        int x = position.x;
        for (unsigned char value : stock_text(text)) {
            if (value < font.first || value > font.last) value = '?';
            const unsigned start = font.word(font.offsets + (value - font.first) * 2);
            const unsigned end = font.word(font.offsets + (value - font.first + 1) * 2);
            for (unsigned row = 0; row < font.height; ++row) {
                const int y = position.y + row;
                if (y < bounds.y1() || y >= bounds.y2()) continue;
                for (unsigned bit = start; bit < end; ++bit) {
                    const int px = x + bit - start;
                    if (px < bounds.x1() || px >= bounds.x2()) continue;
                    if (font.data[font.bitmap + row * font.stride + bit / 8] &
                        (0x80 >> (bit % 8)))
                        native::detail::put_image_pixel(image, bounds, px, y, color);
                }
            }
            x += end - start;
        }
        return true;
    }

    void draw_stock_arrow(native::gpx &graphics, const native::rect &bounds,
                          native::scrollbar_orientation axis,
                          native::scrollbar_part part) {
        if (bounds.w() <= 2 || bounds.h() <= 2) return;
        auto saved = graphics.save_state();
        graphics.set_clip(graphics.get_clip().intersect(native::rect(
            bounds.x1() + 1, bounds.y1() + 1,
            bounds.w() - 2, bounds.h() - 2))).set_pen(1);

        // AtariSTHigh.fnt symbols 1..4 from GEM v1.0.0. Retain the
        // original outlines and shafts even without installed resources.
        static constexpr unsigned char fallback[4][16] = {
            {0x00, 0x00, 0x00, 0x18, 0x3c, 0x66, 0xc3, 0x81,
             0xe7, 0x24, 0x24, 0x24, 0x3c, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x3c, 0x24, 0x24, 0x24, 0xe7,
             0x81, 0xc3, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x30, 0x38, 0x2c, 0xe6, 0x83, 0x83,
             0xe6, 0x2c, 0x38, 0x30, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x0c, 0x1c, 0x34, 0x67, 0xc1, 0xc1,
             0x67, 0x34, 0x1c, 0x0c, 0x00, 0x00, 0x00, 0x00}
        };
        const bool vertical = axis == native::scrollbar_orientation::vertical;
        const bool decrement = part == native::scrollbar_part::decrement;
        const unsigned glyph = vertical ? (decrement ? 1 : 2)
                                        : (decrement ? 4 : 3);
        const auto &font = stock_font();
        const bool loaded = font.bitmap && glyph >= font.first &&
                            glyph <= font.last;
        const unsigned start = loaded
            ? font.word(font.offsets + (glyph - font.first) * 2) : 0;
        const int width = loaded
            ? font.word(font.offsets + (glyph - font.first + 1) * 2) - start
            : 8;
        const int height = loaded ? font.height : 16;
        // AES centers the entire font cell and offsets up by -1,
        // down by 0, and horizontal arrows by +2 pixels vertically.
        const int x = bounds.x1() + (int(bounds.w()) - width) / 2;
        const int y = bounds.y1() + (int(bounds.h()) - height) / 2 +
                      (vertical ? (decrement ? -1 : 0) : 2);
        const auto pixel = [&](int column, int row) {
            const unsigned bit = start + column;
            return loaded
                ? (font.data[font.bitmap + row * font.stride + bit / 8] &
                   (0x80 >> (bit % 8))) != 0
                : (fallback[glyph - 1][row] & (0x80 >> column)) != 0;
        };
        // Emit runs so a remote VDI target needs one call per stroke.
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < width;) {
                if (!pixel(column, row)) { ++column; continue; }
                const int first = column++;
                while (column < width && pixel(column, row)) ++column;
                graphics.draw_line(native::point(x + first, y + row),
                                   native::point(x + column - 1, y + row));
            }
        }
    }
}
