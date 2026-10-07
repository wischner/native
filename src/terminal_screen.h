//
// Declares Vision's bounded ANSI terminal screen and portable pixel
// renderer. The screen is application state, independent of native
// window resources.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>
#include <string>
#include <string_view>
#include <vector>

namespace vision
{
    struct terminal_cell
    {
        char glyph = ' ';
        unsigned ink = 7, paper = 0;
    };
    class terminal_screen
    {
    public:
        // Construct a bounded ASCII grid; invalid dimensions throw.
        terminal_screen(unsigned columns = 64, unsigned rows = 20);
        // Consume incremental ANSI bytes, including split escape
        // sequences.
        void feed(std::string_view bytes);
        // Clear cells, cursor and pending escape/control state.
        void clear();
        // Return checked cell contents for deterministic regression
        // assertions.
        const terminal_cell &get_cell(unsigned column,
                                      unsigned row) const;
        // Render through a portable font into an owned, unfiltered
        // source image, with optional black margins outside the grid.
        std::unique_ptr<native::img>
        render(const native::font_t &font, unsigned margin_x = 0,
               unsigned margin_y = 0) const;

    private:
        // Advance a row, scrolling the bounded grid at the bottom.
        void newline();
        // Write one ASCII cell using current colors, wrapping as
        // needed.
        void put(char glyph);
        // Apply a bounded CSI sequence; unsupported commands do
        // nothing.
        void control(char command);
        unsigned _columns, _rows, _x = 0, _y = 0;
        unsigned _saved_x = 0, _saved_y = 0, _ink = 7, _paper = 0;
        bool _escape = false, _csi = false, _osc = false,
             _osc_escape = false;
        std::string _sequence;
        std::vector<terminal_cell> _cells;
    };
} // namespace vision
