//
// Implements backend-neutral check state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <utility>

#include <native.h>
#include <native/check.h>
#include <native/theme.h>

namespace native
{
    check::check(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    check::check(const std::string &text,
                 const point &position,
                 const size &dimensions)
        : check(text,
                position.x,
                position.y,
                dimensions.w,
                dimensions.h) {}

    check::check(const std::string &text, const rect &bounds)
        : check(text, bounds.p, bounds.d) {}

    check::~check() {
        destroy();
    }

    const std::string &check::get_text() const {
        return _text;
    }

    check &check::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    bool check::get_checked() const {
        return _checked;
    }

    check &check::set_checked(bool checked) {
        if (_checked == checked)
            return *this;
        _checked = checked;
        if (_created)
            apply_checked();
        return *this;
    }

    void check::on_native_checked(bool checked) {
        if (_checked == checked)
            return;
        _checked = checked;
        on_change.emit(_checked);
    }

    void check::draw_control(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        theme::state effective = state;
        effective.selected = _checked;
        draw_background(graphics, appearance, bounds, effective);
        draw_indicator(graphics, appearance, bounds, effective);
        draw_text(graphics, appearance, bounds, effective);
        draw_focus(graphics, appearance, bounds, effective);
    }

    void check::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        appearance.draw_surface(bounds, surface_kind::panel, {});
    }

    void check::draw_indicator(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const int extent = std::max(5, std::min(
            14, static_cast<int>(bounds.d.h) - 4));
        const rect indicator(
            static_cast<coord>(bounds.p.x + 2),
            static_cast<coord>(bounds.p.y +
                (static_cast<int>(bounds.d.h) - extent) / 2),
            static_cast<dim>(extent),
            static_cast<dim>(extent));
        graphics.set_ink(appearance.get_content_background_color())
            .draw_rect(indicator, true)
            .set_ink(appearance.get_button_border_color())
            .draw_rect(indicator, false);
        if (!state.selected)
            return;
        graphics.set_pen(2)
            .set_ink(state.disabled
                ? appearance.get_button_disabled_foreground_color()
                : appearance.get_button_foreground_color())
            .draw_line(
                point(static_cast<coord>(indicator.p.x + 3),
                      static_cast<coord>(indicator.p.y + extent / 2)),
                point(static_cast<coord>(indicator.p.x + extent / 2),
                      static_cast<coord>(indicator.y2() - 3)))
            .draw_line(
                point(static_cast<coord>(indicator.p.x + extent / 2),
                      static_cast<coord>(indicator.y2() - 3)),
                point(static_cast<coord>(indicator.x2() - 3),
                      static_cast<coord>(indicator.p.y + 3)));
    }

    void check::draw_text(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const int extent = std::max(5, std::min(
            14, static_cast<int>(bounds.d.h) - 4));
        const int left = bounds.p.x + extent + 8;
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled
            ? appearance.get_button_disabled_foreground_color()
            : appearance.get_content_foreground_color())
            .draw_text(
                _text,
                rect(static_cast<coord>(left),
                     bounds.p.y,
                     static_cast<dim>(std::max(0, bounds.x2() - left)),
                     bounds.d.h),
                {text_align::start,
                 text_valign::center,
                 text_overflow::ellipsis,
                 true});
    }

    void check::draw_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }
} // namespace native
