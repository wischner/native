//
// Implements the X11 font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Xlib.h>
#include <algorithm>
#include <climits>
#include <initializer_list>

#include <native.h>
#include <native/font.h>
#include "../../portable_font.h"
#include "globals.h"

// font_t on X11: the platform handle (x11_font) owns an X11 core Font
// and lives in linux::x11::font_bindings, keyed by the font's opaque
// uint32_t id.

namespace
{
    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = linux::x11::font_bindings.object_from_handle(id);
        if (f) {
            if (f->owned && f->display && f->xfont)
                XUnloadFont(f->display, f->xfont);
            if (f->metrics)
                XFreeFontInfo(nullptr, f->metrics, 1);
            delete f;
        }
        linux::x11::font_bindings.unregister_by_handle(id);
    }

    uint32_t register_font(Display *display, Font xfont, bool owned) {
        auto *h = new linux::x11::x11_font();
        h->display = display;
        h->xfont = xfont;
        h->metrics =
            display && xfont ? XQueryFont(display, xfont) : nullptr;
        h->owned = owned;
        uint32_t id = next_id();
        linux::x11::font_bindings.register_pair(id, h);
        return id;
    }

    Font load_if_available(Display *display, const char *name) {
        int match_count = 0;
        char **matches = XListFonts(display, name, 1, &match_count);
        if (!matches || match_count == 0) {
            if (matches)
                XFreeFontNames(matches);
            return 0;
        }

        Font font = XLoadFont(display, matches[0]);
        XFreeFontNames(matches);
        return font;
    }

    Font try_load(Display *display,
                  std::initializer_list<const char *> names) {
        for (const char *name : names) {
            Font f = load_if_available(display, name);
            if (f)
                return f;
        }
        return 0;
    }
} // namespace

namespace native
{

    font_t::font_t() = default;

    font_t::font_t(font_t &&other) noexcept
        : _id(other._id)
        , _spec(std::move(other._spec)) {
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
        if (detail::release_portable_font(_id)) {
            _id = 0;
            return;
        }
        if (_id) {
            release(_id);
            _id = 0;
        }
    }

    const font_t &font_t::stock(font_role role) {
        static font_t s[6];
        static bool initialized = false;
        if (!initialized) {
            initialized = true;

            Display *display = linux::x11::cached_display;
            if (!display)
                return s[(int)role];

            const char *x_font = XGetDefault(display, "*", "font");
            Font system_f =
                x_font ? load_if_available(display, x_font) : 0;
            if (!system_f)
                system_f = try_load(
                    display,
                    {"-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
                     "-adobe-helvetica-medium-r-normal--12-120-75-75-p-"
                     "67-iso8859-1",
                     "fixed"});

            Font fixed_f = try_load(display,
                                    {"-misc-fixed-medium-r-normal--13-"
                                     "120-75-75-c-70-iso8859-1",
                                     "fixed"});

            // X11 has no separate role concept; title/small/control
            // share the system font. Stock fonts are not owned; the X
            // server reclaims them on disconnect.
            s[(int)font_role::system]._id =
                register_font(display, system_f, false);
            s[(int)font_role::fixed]._id =
                register_font(display, fixed_f, false);
            s[(int)font_role::icon_label]._id =
                register_font(display, system_f, false);
            s[(int)font_role::title]._id =
                register_font(display, system_f, false);
        s[(int)font_role::small]._id =
                register_font(display, system_f, false);
            s[(int)font_role::control]._id =
                register_font(display, system_f, false);

            s[(int)font_role::system]._spec.family =
                x_font ? x_font : "fixed";
            s[(int)font_role::fixed]._spec.family =
                "-misc-fixed-medium-r-normal--13-120-75-75-c-70-"
                "iso8859-1";
            s[(int)font_role::icon_label]._spec.family =
                s[(int)font_role::system]._spec.family;
            s[(int)font_role::title]._spec.family =
                s[(int)font_role::system]._spec.family;
        s[(int)font_role::small]._spec.family =
                s[(int)font_role::system]._spec.family;
            s[(int)font_role::control]._spec.family =
                s[(int)font_role::system]._spec.family;
            for (auto &font : s)
                font._spec.source = font_source::stock;
        }
        return s[(int)role];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        auto *binding =
            linux::x11::font_bindings.object_from_handle(_id);
        if (!binding || !binding->metrics)
            return {};
        const XFontStruct *font = binding->metrics;
        const int ascent = std::max(1, font->ascent);
        const int descent = std::max(1, font->descent);
        const int leading = 1;
        const font_metrics result{ascent,
                                  descent,
                                  leading,
                                  ascent + descent + leading,
                                  std::max<int>(
                                      1, font->max_bounds.width)};
        return result;
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        auto *binding =
            linux::x11::font_bindings.object_from_handle(_id);
        if (!binding || !binding->metrics)
            return {};
        XFontStruct *font = binding->metrics;
        int direction = 0;
        int ascent = 0;
        int descent = 0;
        XCharStruct extent = {};
        XTextExtents(font,
                     text.data(),
                     static_cast<int>(
                         std::min<std::size_t>(text.size(), INT_MAX)),
                     &direction,
                     &ascent,
                     &descent,
                     &extent);
        const int advance = extent.width;
        const int width = std::max(advance,
                                   static_cast<int>(extent.rbearing) -
                                       extent.lbearing);
        const int height = get_metrics().height;
        return {width, height, advance};
    }

} // namespace native
