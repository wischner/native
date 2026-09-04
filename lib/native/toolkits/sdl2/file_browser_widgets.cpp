//
// Implements compact themed file-browser toolbar icons and a clickable path
// breadcrumb whose leading components elide when horizontal space is tight.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_browser_widgets.h"

#include <algorithm>
#include <utility>

#include <native/font.h>
#include <native/theme.h>

namespace
{
    // Draw a two-segment chevron centered around one point.
    void draw_chevron(native::gpx &graphics,
                      native::point center,
                      int direction) {
        graphics
            .draw_line(
                native::point(
                    static_cast<native::coord>(center.x + direction * 3),
                    static_cast<native::coord>(center.y - 4)),
                native::point(
                    static_cast<native::coord>(center.x - direction),
                    center.y))
            .draw_line(
                native::point(
                    static_cast<native::coord>(center.x - direction),
                    center.y),
                native::point(
                    static_cast<native::coord>(center.x + direction * 3),
                    static_cast<native::coord>(center.y + 4)));
    }
} // namespace

namespace linux::sdl2
{
    file_browser_icon_button::file_browser_icon_button(
        file_browser_button_icon icon,
        const native::rect &bounds)
        : native::button({}, bounds)
        , _icon(icon) {}

    void file_browser_icon_button::draw_background(
        native::gpx &graphics,
        native::theme &appearance,
        const native::rect &bounds,
        const native::theme::state &state) {
        const native::theme::palette colors = appearance.native_palette();
        native::rgba color = colors.button_bg;
        if (state.pressed)
            color = colors.button_pressed_bg;
        else if (state.hot)
            color = colors.button_hot_bg;
        graphics.set_ink(color).draw_rect(bounds, true);
    }

    void file_browser_icon_button::draw_border(
        native::gpx &graphics,
        native::theme &appearance,
        const native::rect &bounds,
        const native::theme::state &state) {
        if (state.hot || state.pressed) {
            graphics.set_pen(1)
                .set_ink(appearance.native_palette().button_border)
                .draw_rect(bounds, false);
        }
    }

    void file_browser_icon_button::draw_text(
        native::gpx &graphics,
        native::theme &appearance,
        const native::rect &bounds,
        const native::theme::state &state) {
        const native::theme::palette colors = appearance.native_palette();
        native::rgba color = colors.button_text;
        if (state.pressed)
            color = colors.button_pressed_text;
        else if (state.hot)
            color = colors.button_hot_text;
        const int cx = bounds.p.x + static_cast<int>(bounds.d.w) / 2;
        const int cy = bounds.p.y + static_cast<int>(bounds.d.h) / 2;
        graphics.set_pen(2).set_ink(color);
        switch (_icon) {
        case file_browser_button_icon::back:
            graphics.draw_line(
                native::point(cx - 5, cy), native::point(cx + 5, cy));
            draw_chevron(graphics, native::point(cx - 4, cy), 1);
            break;
        case file_browser_button_icon::forward:
            graphics.draw_line(
                native::point(cx - 5, cy), native::point(cx + 5, cy));
            draw_chevron(graphics, native::point(cx + 4, cy), -1);
            break;
        case file_browser_button_icon::up:
            graphics.draw_line(
                native::point(cx, cy + 5), native::point(cx, cy - 5));
            graphics
                .draw_line(native::point(cx, cy - 5),
                           native::point(cx - 4, cy - 1))
                .draw_line(native::point(cx, cy - 5),
                           native::point(cx + 4, cy - 1));
            break;
        }
        graphics.set_pen(1);
    }

    file_browser_breadcrumb::file_browser_breadcrumb(
        const native::rect &bounds)
        : native::canvas(bounds) {
        set_horizontal_scrollbar_policy(native::scrollbar_policy::never);
        set_vertical_scrollbar_policy(native::scrollbar_policy::never);
        on_wnd_paint.connect([this](native::wnd_paint_event event) {
            paint(event);
            return true;
        });
        on_mouse_click.connect([this](native::mouse_event event) {
            return click(event);
        });
    }

    file_browser_breadcrumb &file_browser_breadcrumb::set_path(
        const std::filesystem::path &path) {
        _path = path.lexically_normal();
        invalidate();
        return *this;
    }

    void file_browser_breadcrumb::paint(
        native::wnd_paint_event event) {
        auto appearance = native::theme::create(event.g);
        const native::rect bounds(
            0, 0, get_dimensions().w, get_dimensions().h);
        appearance->draw_surface(
            bounds, native::surface_kind::inset, {});

        std::vector<segment> desired;
        std::filesystem::path target = _path.root_path();
        if (!target.empty())
            desired.push_back({target.string(), target, {}});
        for (const auto &component : _path.relative_path()) {
            target /= component;
            desired.push_back({component.string(), target, {}});
        }
        if (desired.empty())
            desired.push_back({_path.string(), _path, {}});

        const native::font_t &font =
            native::font_t::stock(native::font_role::control);
        std::vector<int> widths;
        widths.reserve(desired.size());
        int total = 4;
        for (const segment &item : desired) {
            const int width = std::max(
                28, font.measure_text(item.label).width + 22);
            widths.push_back(width);
            total += width;
        }

        std::size_t first = 0;
        const int available = static_cast<int>(bounds.d.w) - 4;
        if (total > available && desired.size() > 1) {
            int used = 32;
            first = desired.size();
            while (first > 0 && used + widths[first - 1] <= available) {
                --first;
                used += widths[first];
            }
        }

        _segments.clear();
        int x = 2;
        if (first > 0) {
            event.g.set_font(font)
                .set_ink(appearance->native_palette().content_text)
                .draw_text(
                    "...",
                    native::rect(2, 0, 28, bounds.d.h),
                    {native::text_align::center,
                     native::text_valign::center,
                     native::text_overflow::clip,
                     false});
            x = 32;
        }
        for (std::size_t index = first;
             index < desired.size(); ++index) {
            const int width = std::min(
                widths[index], std::max(0, available - x));
            if (width <= 0)
                break;
            desired[index].bounds = native::rect(
                static_cast<native::coord>(x), 1,
                static_cast<native::dim>(width),
                static_cast<native::dim>(
                    std::max(0, static_cast<int>(bounds.d.h) - 2)));
            _segments.push_back(desired[index]);
            const int text_width = std::max(0, width - 14);
            event.g.set_font(font)
                .set_ink(appearance->native_palette().content_text)
                .draw_text(
                    desired[index].label,
                    native::rect(
                        static_cast<native::coord>(x + 5), 1,
                        static_cast<native::dim>(text_width),
                        static_cast<native::dim>(
                            std::max(0,
                                static_cast<int>(bounds.d.h) - 2))),
                    {native::text_align::start,
                     native::text_valign::center,
                     native::text_overflow::ellipsis,
                     false});
            if (index + 1 < desired.size()) {
                const int separator_x = x + width - 7;
                event.g.set_pen(1)
                    .set_ink(appearance->native_palette().separator)
                    .draw_line(
                        native::point(separator_x - 2, 9),
                        native::point(separator_x + 1, 13))
                    .draw_line(
                        native::point(separator_x + 1, 13),
                        native::point(separator_x - 2, 17));
            }
            x += width;
        }
    }

    bool file_browser_breadcrumb::click(native::mouse_event event) {
        if (event.button != native::mouse_button::left ||
            event.action != native::mouse_action::release) {
            return false;
        }
        for (const segment &item : _segments) {
            if (item.bounds.contains(event.position)) {
                on_navigate.emit(item.target);
                return true;
            }
        }
        return false;
    }
} // namespace linux::sdl2
