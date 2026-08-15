//
// Implements the OpenMotif font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <climits>
#include <initializer_list>

#include <native.h>
#include "globals.h"

// font_t on OpenMotif: same X11 core font strategy, using linux::openmotif::font_bindings.

namespace
{
    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = linux::openmotif::font_bindings.object_from_handle(id);
        if (f) {
            if (f->owned && f->display && f->xfont)
                XUnloadFont(f->display, f->xfont);
            if (f->metrics)
                XFreeFontInfo(nullptr, f->metrics, 1);
            delete f;
        }
        linux::openmotif::font_bindings.unregister_by_handle(id);
    }

    uint32_t register_font(Display *display, Font xfont, bool owned) {
        auto *h = new linux::openmotif::motif_font();
        h->display = display;
        h->xfont   = xfont;
        h->metrics = display && xfont ? XQueryFont(display, xfont) : nullptr;
        h->owned   = owned;
        uint32_t id = next_id();
        linux::openmotif::font_bindings.register_pair(id, h);
        return id;
    }

    Font try_load(Display *display, std::initializer_list<const char *> names) {
        for (const char *name : names) {
            Font f = XLoadFont(display, name);
            if (f) return f;
        }
        return 0;
    }
}

namespace native
{

font_t::font_t() = default;

font_t::font_t(font_t &&other) noexcept
    : _id(other._id), _spec(std::move(other._spec)) {
    other._id = 0;
}

font_t &font_t::operator=(font_t &&other) noexcept {
    if (this != &other) {
        std::swap(_id, other._id);
        _spec = std::move(other._spec);
    }
    return *this;
}

font_t::~font_t() {
    if (_id) {
        release(_id);
        _id = 0;
    }
}

font_t font_t::create(const font_spec &spec) {
    font_t f;
    Display *display = linux::openmotif::cached_display;
    if (!display) return f;

    Font xfont = spec.name.empty()
        ? try_load(display, { "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*", "fixed" })
        : XLoadFont(display, spec.name.c_str());

    if (!xfont) return f;
    f._id = register_font(display, xfont, true);
    f._spec = spec;
    return f;
}

const font_t &font_t::stock(font_role role) {
    static font_t s[5];
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        Display *display = linux::openmotif::cached_display;
        if (!display) return s[(int)role];

        const char *x_font = XGetDefault(display, "*", "font");
        Font system_f = x_font ? XLoadFont(display, x_font) : 0;
        if (!system_f)
            system_f = try_load(display, {
                "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
                "-adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1",
                "fixed"
            });

        Font fixed_f = try_load(display, {
            "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1",
            "fixed"
        });

        s[(int)font_role::system]._id  = register_font(display, system_f, false);
        s[(int)font_role::fixed]._id   = register_font(display, fixed_f,  false);
        s[(int)font_role::title]._id   = register_font(display, system_f, false);
        s[(int)font_role::small_]._id  = register_font(display, system_f, false);
        s[(int)font_role::control]._id = register_font(display, system_f, false);

        s[(int)font_role::system]._spec.name  = x_font ? x_font : "fixed";
        s[(int)font_role::fixed]._spec.name =
            "-misc-fixed-medium-r-normal--13-120-75-75-c-70-"
            "iso8859-1";
        s[(int)font_role::title]._spec.name   = s[(int)font_role::system]._spec.name;
        s[(int)font_role::small_]._spec.name  = s[(int)font_role::system]._spec.name;
        s[(int)font_role::control]._spec.name = s[(int)font_role::system]._spec.name;
    }
    return s[(int)role];
}

font_metrics font_t::get_metrics() const {
    auto *binding = linux::openmotif::font_bindings.object_from_handle(_id);
    if (!binding || !binding->metrics)
        return {};
    const XFontStruct *font = binding->metrics;
    const font_metrics result{
        font->ascent,
        font->descent,
        0,
        font->ascent + font->descent,
        font->max_bounds.width};
    return result;
}

text_metrics font_t::measure_text(const std::string &text) const {
    auto *binding = linux::openmotif::font_bindings.object_from_handle(_id);
    if (!binding || !binding->metrics)
        return {};
    XFontStruct *font = binding->metrics;
    int direction = 0;
    int ascent = 0;
    int descent = 0;
    XCharStruct extent = {};
    XTextExtents(
        font,
        text.data(),
        static_cast<int>(std::min<std::size_t>(text.size(), INT_MAX)),
        &direction,
        &ascent,
        &descent,
        &extent);
    const int advance = extent.width;
    const int width = std::max(
        advance,
        static_cast<int>(extent.rbearing) - extent.lbearing);
    const int height = font->ascent + font->descent;
    return {width, height, advance};
}

} // namespace native
