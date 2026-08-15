//
// Implements shared graphics-context drawing state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/graphics.h>

namespace native
{
    gpx::~gpx() = default;

    gpx &gpx::set_ink(rgba c) {
        _ink = c;
        return *this;
    }

    rgba gpx::get_ink() const {
        return _ink;
    }

    gpx &gpx::set_paper(rgba c) {
        _paper = c;
        return *this;
    }

    rgba gpx::get_paper() const {
        return _paper;
    }

    gpx &gpx::set_pen(uint8_t t) {
        _thickness = t;
        return *this;
    }

    uint8_t gpx::get_pen() const {
        return _thickness;
    }

    gpx &gpx::set_font(const font_t &f) {
        _font = &f;
        return *this;
    }

    const font_t &gpx::get_font() const {
        if (_font)
            return *_font;
        return font_t::stock(font_role::system);
    }

    font_metrics gpx::get_font_metrics() const {
        return get_font().get_metrics();
    }

    text_metrics gpx::measure_text(const std::string &text) const {
        return get_font().measure_text(text);
    }

    text_metrics gpx::measure_character(char32_t character) const {
        return get_font().measure_character(character);
    }

} // namespace native
