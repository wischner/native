//
// Implements bounded ASCII/ANSI parsing and image rendering for
// Vision's terminal test. Unsupported control sequences are consumed
// without drawing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "terminal_screen.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>

namespace
{
    const std::array<native::rgba, 16> palette = {
        native::rgba(0, 0, 0, 255),
        native::rgba(170, 0, 0, 255),
        native::rgba(0, 170, 0, 255),
        native::rgba(170, 85, 0, 255),
        native::rgba(0, 0, 170, 255),
        native::rgba(170, 0, 170, 255),
        native::rgba(0, 170, 170, 255),
        native::rgba(170, 170, 170, 255),
        native::rgba(85, 85, 85, 255),
        native::rgba(255, 85, 85, 255),
        native::rgba(85, 255, 85, 255),
        native::rgba(255, 255, 85, 255),
        native::rgba(85, 85, 255, 255),
        native::rgba(255, 85, 255, 255),
        native::rgba(85, 255, 255, 255),
        native::rgba(255, 255, 255, 255)};
}
namespace vision
{
    terminal_screen::terminal_screen(unsigned columns, unsigned rows)
        : _columns(columns), _rows(rows) {
        if (!columns || !rows || columns > 160 || rows > 100)
            throw std::invalid_argument(
                "Invalid terminal grid dimensions.");
        _cells.resize(columns * rows);
    }
    void terminal_screen::clear() {
        _x = _y = _saved_x = _saved_y = 0;
        _ink = 7;
        _paper = 0;
        _escape = _csi = _osc = _osc_escape = false;
        _sequence.clear();
        std::fill(_cells.begin(), _cells.end(), terminal_cell{});
    }
    const terminal_cell &terminal_screen::get_cell(unsigned column,
                                                   unsigned row) const {
        if (column >= _columns || row >= _rows)
            throw std::out_of_range("Terminal cell outside grid.");
        return _cells.at(row * _columns + column);
    }
    void terminal_screen::newline() {
        if (++_y < _rows)
            return;
        std::move(_cells.begin() + _columns, _cells.end(),
                  _cells.begin());
        std::fill(_cells.end() - _columns, _cells.end(),
                  terminal_cell{' ', _ink, _paper});
        _y = _rows - 1;
    }
    void terminal_screen::put(char glyph) {
        if (_x >= _columns) {
            _x = 0;
            newline();
        }
        _cells[_y * _columns + _x++] = {glyph, _ink, _paper};
    }
    void terminal_screen::control(char command) {
        std::vector<unsigned> parameters;
        for (std::size_t start = 0; start <= _sequence.size();) {
            const auto end = _sequence.find(';', start);
            const auto length =
                (end == std::string::npos ? _sequence.size() : end) -
                start;
            unsigned value = 0;
            if (length) {
                const char *first = _sequence.data() + start;
                auto result =
                    std::from_chars(first, first + length, value);
                if (result.ec != std::errc{} ||
                    result.ptr != first + length)
                    return;
            }
            parameters.push_back(value);
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        const auto amount =
            std::min(1000u, std::max(1u, parameters[0]));
        const terminal_cell blank{' ', _ink, _paper};
        const auto cursor =
            std::size_t(_y) * _columns + std::min(_x, _columns - 1);
        if (command == 'm')
            for (unsigned value : parameters) {
                if (!value) {
                    _ink = 7;
                    _paper = 0;
                } else if (value == 1)
                    _ink |= 8;
                else if (value == 22)
                    _ink &= 7;
                else if (value == 39)
                    _ink = 7;
                else if (value == 49)
                    _paper = 0;
                else if (value >= 30 && value <= 37)
                    _ink = (value - 30) | (_ink & 8);
                else if (value >= 40 && value <= 47)
                    _paper = value - 40;
                else if (value >= 90 && value <= 97)
                    _ink = value - 90 + 8;
                else if (value >= 100 && value <= 107)
                    _paper = value - 100 + 8;
            }
        else if (command == 'H' || command == 'f') {
            _y = std::min(amount - 1, _rows - 1);
            _x = std::min((parameters.size() > 1
                               ? std::max(1u, parameters[1])
                               : 1u) -
                              1,
                          _columns - 1);
        } else if (command == 'A')
            _y -= std::min(_y, amount);
        else if (command == 'B')
            _y = std::min(_rows - 1, _y + amount);
        else if (command == 'C')
            _x = std::min(_columns - 1, _x + amount);
        else if (command == 'D')
            _x -= std::min(_x, amount);
        else if (command == 'G')
            _x = std::min(amount - 1, _columns - 1);
        else if (command == 's') {
            _saved_x = _x;
            _saved_y = _y;
        } else if (command == 'u') {
            _x = _saved_x;
            _y = _saved_y;
        } else if (command == 'J') {
            if (parameters[0] == 2 || parameters[0] == 3)
                std::fill(_cells.begin(), _cells.end(), blank);
            else if (parameters[0] == 0)
                std::fill(_cells.begin() + cursor, _cells.end(), blank);
            else if (parameters[0] == 1)
                std::fill(_cells.begin(), _cells.begin() + cursor + 1,
                          blank);
        } else if (command == 'K') {
            const auto first = _cells.begin() + _y * _columns;
            if (parameters[0] == 0)
                std::fill(_cells.begin() + cursor, first + _columns,
                          blank);
            else if (parameters[0] == 1)
                std::fill(first, _cells.begin() + cursor + 1, blank);
            else if (parameters[0] == 2)
                std::fill(first, first + _columns, blank);
        }
    }
    void terminal_screen::feed(std::string_view bytes) {
        for (unsigned char byte : bytes) {
            if (_osc) {
                if (byte == 7 || (_osc_escape && byte == '\\'))
                    _osc = false;
                _osc_escape = byte == 27;
                continue;
            }
            if (byte == 27) {
                _escape = true;
                _csi = false;
                _sequence.clear();
                continue;
            }
            if (_escape) {
                _escape = false;
                if (byte == '[')
                    _csi = true;
                else if (byte == ']') {
                    _osc = true;
                    _osc_escape = false;
                } else if (byte == 'c')
                    clear();
                continue;
            }
            if (_csi) {
                if (byte >= 0x40 && byte <= 0x7e) {
                    control(static_cast<char>(byte));
                    _csi = false;
                } else if (_sequence.size() < 128)
                    _sequence += static_cast<char>(byte);
                else {
                    _sequence.clear();
                    _csi = false;
                }
                continue;
            }
            if (byte == '\r')
                _x = 0;
            else if (byte == '\n') {
                _x = 0;
                newline();
            } else if (byte == '\b')
                _x -= std::min(_x, 1u);
            else if (byte == '\t') {
                const unsigned spaces = 8 - (_x % 8);
                for (unsigned i = 0; i < spaces; ++i)
                    put(' ');
            } else if (byte >= 32 && byte <= 126)
                put(static_cast<char>(byte));
            else if (byte >= 0xc0)
                put('?');
        }
    }
    std::unique_ptr<native::img>
    terminal_screen::render(const native::font_t &font,
                            unsigned margin_x, unsigned margin_y) const {
        if (margin_x > 128 || margin_y > 128)
            throw std::invalid_argument("Terminal margins exceed 128 pixels.");
        auto image = std::make_unique<native::img>(
            static_cast<native::dim>(_columns * 8 + 2 * margin_x),
            static_cast<native::dim>(_rows * 8 + 2 * margin_y));
        auto &graphics = image->get_gpx();
        graphics.set_font(font)
            .set_ink(palette[0])
            .draw_rect(native::rect(0, 0, image->w(), image->h()),
                       true);
        for (unsigned row = 0; row < _rows; ++row) {
            for (unsigned column = 0; column < _columns;) {
                const auto &first = get_cell(column, row);
                unsigned end = column;
                std::string text;
                while (end < _columns) {
                    const auto &cell = get_cell(end, row);
                    if (cell.ink != first.ink ||
                        cell.paper != first.paper)
                        break;
                    text += cell.glyph;
                    ++end;
                }
                const native::rect bounds(
                    static_cast<native::coord>(column * 8 + margin_x),
                    static_cast<native::coord>(row * 8 + margin_y),
                    static_cast<native::dim>((end - column) * 8), 8);
                graphics.set_ink(palette[first.paper])
                    .draw_rect(bounds, true);
                graphics.set_clip(bounds)
                    .set_ink(palette[first.ink])
                    .draw_text(text, bounds.p);
                graphics.set_clip(
                    native::rect(0, 0, image->w(), image->h()));
                column = end;
            }
        }
        return image;
    }
} // namespace vision
