//
// Implements a themed bottom-edge status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <typeinfo>
#include <utility>

#include <native/graphics.h>
#include <native/status_bar.h>

#include "status_bar_peer.h"

namespace native
{
    status_bar::status_bar(wnd &owner, int height)
        : non_client(owner, window_edge::bottom, height)
        , _native_peer(detail::create_status_bar_peer()) {}

    status_bar::~status_bar() = default;

    const std::string &status_bar::get_text() const { return _text; }

    status_bar &status_bar::set_text(const std::string &text) {
        if (_parts.empty() && _text == text)
            return *this;
        _text = text;
        _parts.clear();
        synchronize_native(get_bounds());
        invalidate();
        return *this;
    }

    const std::vector<status_bar_part> &status_bar::get_parts() const {
        return _parts;
    }

    status_bar &status_bar::set_parts(std::vector<status_bar_part> parts) {
        for (status_bar_part &part : parts)
            part.width = std::max(0, part.width);

        const bool unchanged = _text.empty() &&
            _parts.size() == parts.size() &&
            std::equal(_parts.begin(), _parts.end(), parts.begin(),
                [](const status_bar_part &left,
                   const status_bar_part &right) {
                    return left.text == right.text &&
                        left.width == right.width;
                });
        if (unchanged)
            return *this;

        _parts = std::move(parts);
        _text.clear();
        synchronize_native(get_bounds());
        invalidate();
        return *this;
    }

    void status_bar::draw(gpx &graphics, const rect &bounds) {
        if (!bounds.w() || !bounds.h())
            return;
        if (synchronize_native(bounds))
            return;
        auto saved = graphics.save_state();
        auto appearance = theme::create(graphics);
        const theme::state state{};
        draw_background(graphics, *appearance, bounds, state);

        std::vector<status_bar_part> parts = _parts;
        if (parts.empty())
            parts.push_back({_text, 0});

        int fixed = 0;
        int flexible = 0;
        for (const status_bar_part &part : parts) {
            if (part.width > 0)
                fixed += part.width;
            else
                ++flexible;
        }
        int remaining = std::max(0, static_cast<int>(bounds.w())-fixed);
        int x = bounds.x1();
        int flexible_left = flexible;
        for (const status_bar_part &part : parts) {
            int width = part.width;
            if (!width && flexible_left > 0) {
                width = remaining / flexible_left;
                remaining -= width;
                --flexible_left;
            }
            width = std::max(0, std::min(width,
                static_cast<int>(bounds.x2())-x));
            draw_part(graphics, *appearance,
                      rect(static_cast<coord>(x), bounds.y1(),
                           static_cast<dim>(width), bounds.h()),
                      part, state);
            x += width;
        }
    }

    void status_bar::on_configuration_changed() {
        synchronize_native(get_bounds());
    }

    bool status_bar::synchronize_native(const rect &bounds) {
        // A derived status bar may override the protected drawing stages.
        // Keep that extension contract by enabling a native peer only for
        // the exact base control.
        return typeid(*this) == typeid(status_bar) &&
            _native_peer && _native_peer->update(*this, bounds);
    }

    void status_bar::draw_background(gpx &,
                                     theme &appearance,
                                     const rect &bounds,
                                     const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::status, state);
    }

    void status_bar::draw_part(gpx &graphics,
                               theme &appearance,
                               const rect &bounds,
                               const status_bar_part &part,
                               const theme::state &state) {
        if (!bounds.w() || !bounds.h())
            return;
        appearance.draw_surface(bounds, surface_kind::status_part, state);
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(colors.button_text);
        const rect text_bounds(
            static_cast<coord>(bounds.x1()+5), bounds.y1(),
            static_cast<dim>(std::max(0, static_cast<int>(bounds.w())-10)),
            bounds.h());
        graphics.draw_text(part.text, text_bounds,
                           {text_align::start, text_valign::center,
                            text_overflow::ellipsis, true});
    }
} // namespace native
