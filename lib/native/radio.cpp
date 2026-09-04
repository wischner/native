//
// Implements backend-neutral radio state and grouping.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <utility>

#include <native/radio.h>
#include <native/theme.h>

namespace native
{
    radio::radio(
        std::string text, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _text(std::move(text)) {}

    radio::radio(const std::string &text,
                 const point &position,
                 const size &dimensions)
        : radio(text,
                position.x,
                position.y,
                dimensions.w,
                dimensions.h) {}

    radio::radio(const std::string &text, const rect &bounds)
        : radio(text, bounds.p, bounds.d) {}

    radio::~radio() {
        destroy();
    }

    const std::string &radio::get_text() const {
        return _text;
    }

    radio &radio::set_text(const std::string &text) {
        _text = text;
        if (_created)
            apply_text();
        return *this;
    }

    bool radio::get_selected() const {
        return _selected;
    }

    radio &radio::set_selected(bool selected) {
        if (!selected) {
            if (!_selected)
                return *this;
            _selected = false;
            if (_created)
                apply_selected();
            return *this;
        }
        select_exclusive(false);
        return *this;
    }

    void radio::on_native_selected() {
        select_exclusive(true);
    }

    void radio::select_exclusive(bool notify) {
        if (!_parent) {
            const bool changed = !_selected;
            _selected = true;
            if (_created)
                apply_selected();
            if (notify && changed)
                selection_changed(true);
            return;
        }

        for (wnd *sibling : _parent->_children) {
            auto *other = dynamic_cast<radio *>(sibling);
            if (!other || other == this || !other->_selected) {
                continue;
            }
            other->_selected = false;
            if (other->_created)
                other->apply_selected();
            if (notify)
                other->selection_changed(false);
        }
        const bool changed = !_selected;
        _selected = true;
        if (_created)
            apply_selected();
        if (notify && changed)
            selection_changed(true);
    }

    void radio::selection_changed(bool selected) {
        on_change.emit(selected);
    }

    void radio::draw_control(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        theme::state effective = state;
        effective.selected = _selected;
        draw_background(graphics, appearance, bounds, effective);
        draw_indicator(graphics, appearance, bounds, effective);
        draw_text(graphics, appearance, bounds, effective);
        draw_focus(graphics, appearance, bounds, effective);
    }

    void radio::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        appearance.draw_surface(bounds, surface_kind::panel, {});
    }

    void radio::draw_indicator(
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
            .draw_ellipse(indicator, true)
            .set_ink(appearance.get_button_border_color())
            .draw_ellipse(indicator, false);
        if (!state.selected)
            return;
        const int inset = std::max(2, extent / 3);
        graphics.set_ink(state.disabled
            ? appearance.get_button_disabled_foreground_color()
            : appearance.get_button_foreground_color())
            .draw_ellipse(
                rect(static_cast<coord>(indicator.p.x + inset),
                     static_cast<coord>(indicator.p.y + inset),
                     static_cast<dim>(std::max(1, extent - inset * 2)),
                     static_cast<dim>(std::max(1, extent - inset * 2))),
                true);
    }

    void radio::draw_text(
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

    void radio::draw_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }
} // namespace native
