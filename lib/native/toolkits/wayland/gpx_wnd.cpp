//
// Draws into the frame-owned Cairo context supplied by GTK4.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../gpx_wnd.h"
#include <stdexcept>
namespace
{
    cairo_t *context(native::wnd &owner,
                     native::rect clip,
                     native::point offset,
                     native::rgba ink,
                     int thickness) {
        auto *cr = linux::wayland::state(owner).context;
        if (!cr)
            throw std::runtime_error(
                "Wayland window graphics require a paint callback.");
        cairo_save(cr);
        cairo_translate(cr, offset.x, offset.y);
        cairo_rectangle(cr, clip.x1(), clip.y1(), clip.w(), clip.h());
        cairo_clip(cr);
        cairo_set_source_rgba(
            cr, ink.r / 255., ink.g / 255., ink.b / 255., ink.a / 255.);
        cairo_set_line_width(cr, thickness);
        return cr;
    }
} // namespace
namespace native
{
    gpx_wnd::gpx_wnd(const wnd *owner, point offset)
        : _wnd(const_cast<wnd *>(owner))
        , _clip(point(), owner->get_dimensions())
        , _offset(offset) {}
    gpx_wnd::~gpx_wnd() = default;
    wnd *gpx_wnd::window() const {
        return _wnd;
    }
    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }
    rect gpx_wnd::get_clip() const {
        return _clip;
    }
    gpx &gpx_wnd::clear(rgba color) {
        auto *cr = context(*_wnd, _clip, _offset, color, _thickness);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_restore(cr);
        return *this;
    }
    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cr = context(*_wnd, _clip, _offset, _ink, _thickness);
        cairo_move_to(cr, from.x + .5, from.y + .5);
        cairo_line_to(cr, to.x + .5, to.y + .5);
        cairo_stroke(cr);
        cairo_restore(cr);
        return *this;
    }
    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        auto *cr = context(*_wnd, _clip, _offset, _ink, _thickness);
        cairo_rectangle(cr,
                        r.x1() + (filled ? 0 : .5),
                        r.y1() + (filled ? 0 : .5),
                        r.w() - (filled ? 0 : 1),
                        r.h() - (filled ? 0 : 1));
        if (filled)
            cairo_fill(cr);
        else
            cairo_stroke(cr);
        cairo_restore(cr);
        return *this;
    }
    gpx &gpx_wnd::draw_native_text(const std::string &value, point p) {
        auto *cr = context(*_wnd, _clip, _offset, _ink, _thickness);
        linux::wayland::text(cr, value, p, _font, _ink);
        cairo_restore(cr);
        return *this;
    }
    gpx &gpx_wnd::draw_img(const img &src, point p) {
        auto *cr = context(*_wnd, _clip, _offset, _ink, _thickness);
        linux::wayland::image(cr, src, p);
        cairo_restore(cr);
        return *this;
    }
} // namespace native
