//
// Implements backend-neutral button state and property behavior.
// Backends provide creation, destruction, display, and label updates.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <utility>

#include <native/button.h>
#include <native/font.h>
#include <native/theme.h>

namespace native
{
    button::button(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    button::button(const std::string &text,
                   const point &position,
                   const size &dimensions)
        : button(text,
                 position.x,
                 position.y,
                 dimensions.w,
                 dimensions.h) {}

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p, bounds.d) {}

    button::~button() {
        destroy();
    }

    const std::string &button::get_text() const {
        return _text;
    }

    button &button::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    void button::on_native_click() {
        on_click.emit();
    }

    void button::draw_control(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        draw_background(graphics, appearance, bounds, state);
        draw_border(graphics, appearance, bounds, state);
        draw_text(graphics, appearance, bounds, state);
        draw_focus(graphics, appearance, bounds, state);
    }

    void button::draw_background(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        rgba color = appearance.get_button_background_color();
        if (state.pressed)
            color = appearance.get_button_pressed_background_color();
        else if (state.hot)
            color = appearance.get_button_hot_background_color();
        graphics.set_ink(color).draw_rect(bounds, true);
    }

    void button::draw_border(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        graphics.set_pen(1)
            .set_ink(appearance.get_button_border_color())
            .draw_rect(bounds, false);
    }

    void button::draw_text(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        rgba color = state.disabled
                         ? appearance.get_button_disabled_foreground_color()
                         : appearance.get_button_foreground_color();
        if (!state.disabled && state.pressed)
            color = appearance.get_button_pressed_foreground_color();
        else if (!state.disabled && state.hot)
            color = appearance.get_button_hot_foreground_color();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(color).draw_text(
            _text,
            bounds,
            {text_align::center,
             text_valign::center,
             text_overflow::ellipsis,
             true});
    }

    void button::draw_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const int inset = std::max(1, appearance.get_focus_inset());
        const int width = std::max(0,
            static_cast<int>(bounds.d.w) - inset * 2);
        const int height = std::max(0,
            static_cast<int>(bounds.d.h) - inset * 2);
        appearance.draw_focus(
            rect(static_cast<coord>(bounds.p.x + inset),
                 static_cast<coord>(bounds.p.y + inset),
                 static_cast<dim>(width),
                 static_cast<dim>(height)),
            state);
    }
} // namespace native
