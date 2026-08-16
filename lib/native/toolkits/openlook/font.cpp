//
// Implements XView stock fonts and X11 core-font measurement while
// portable TrueType fonts remain owned by the shared font engine.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <climits>
#include <initializer_list>
#include <utility>

#include <native.h>
#include <native/font.h>

#include <X11/Xlib.h>
#include <xview/font.h>
#include <xview/window.h>
#include <xview/xview.h>

#include "../../portable_font.h"
#include "globals.h"

namespace
{
    std::uint32_t next_id() {
        static std::uint32_t current = 0;
        return ++current;
    }

    void release(std::uint32_t id) {
        auto *font = linux::openlook::font_bindings
                         .object_from_handle(id);
        if (font) {
            if (font->owned && font->display && font->xfont)
                XUnloadFont(font->display, font->xfont);
            if (font->metrics)
                XFreeFontInfo(nullptr, font->metrics, 1);
            delete font;
        }
        linux::openlook::font_bindings.unregister_by_handle(id);
    }

    std::uint32_t register_font(Display *display,
                                Font xfont,
                                bool owned) {
        auto *font = new linux::openlook::openlook_font;
        font->display = display;
        font->xfont = xfont;
        font->metrics = display && xfont
                            ? XQueryFont(display, xfont)
                            : nullptr;
        font->owned = owned;
        const std::uint32_t id = next_id();
        linux::openlook::font_bindings.register_pair(id, font);
        return id;
    }

    Font load_if_available(Display *display, const char *name) {
        int count = 0;
        char **matches = XListFonts(display, name, 1, &count);
        if (!matches || count == 0) {
            if (matches)
                XFreeFontNames(matches);
            return 0;
        }
        const Font font = XLoadFont(display, matches[0]);
        XFreeFontNames(matches);
        return font;
    }

    Font try_load(Display *display,
                  std::initializer_list<const char *> names) {
        for (const char *name : names) {
            const Font font = load_if_available(display, name);
            if (font)
                return font;
        }
        return 0;
    }

    Font control_font(Font fallback) {
        native::app_wnd *main_window = native::app::main_wnd();
        auto *state = main_window
                          ? linux::openlook::window_state(main_window)
                          : nullptr;
        if (!state || !state->content)
            return fallback;
        Xv_Font resource = static_cast<Xv_Font>(xv_get(
            state->content, WIN_FONT));
        auto *information =
            resource
                ? reinterpret_cast<XFontStruct *>(xv_get(
                      resource, FONT_INFO))
                : nullptr;
        return information ? information->fid : fallback;
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
        static font_t fonts[6];
        static bool ready = false;
        Display *display = linux::openlook::cached_display;
        if (!display) {
            static font_t fallback[6];
            static bool fallback_ready = false;
            if (!fallback_ready) {
                fallback_ready = true;
                constexpr int sizes[] = {13, 13, 13, 14, 11, 13};
                for (const auto &description : enumerate_installed()) {
                    bool valid = true;
                    for (int index = 0; index < 6; ++index) {
                        fallback[index] = from_file(
                            description.path,
                            sizes[index],
                            description.face_index);
                        valid = valid && fallback[index].valid();
                    }
                    if (!valid)
                        continue;
                    for (auto &font : fallback)
                        font._spec.source = font_source::stock;
                    break;
                }
            }
            return fallback[static_cast<int>(role)];
        }

        if (!ready) {
            ready = true;
            Font system = try_load(
                display,
                {"-*-lucida-medium-r-normal-*-12-*-*-*-*-*-*-*",
                 "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
                 "fixed"});
            Font fixed = try_load(
                display,
                {"-misc-fixed-medium-r-normal--13-120-75-75-c-70-"
                 "iso8859-1",
                 "fixed"});
            const Font control = control_font(system);

            const Font selected[] = {
                system, fixed, control, system, system, control};
            const char *families[] = {
                "OPEN LOOK system",
                "fixed",
                "XView control",
                "OPEN LOOK system",
                "OPEN LOOK system",
                "XView control"};
            for (int index = 0; index < 6; ++index) {
                fonts[index]._id = register_font(
                    display, selected[index], false);
                fonts[index]._spec.family = families[index];
                fonts[index]._spec.source = font_source::stock;
            }
        }
        return fonts[static_cast<int>(role)];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        auto *binding = linux::openlook::font_bindings
                            .object_from_handle(_id);
        if (!binding || !binding->metrics)
            return {};
        const XFontStruct *font = binding->metrics;
        const int ascent = std::max(1, font->ascent);
        const int descent = std::max(1, font->descent);
        constexpr int leading = 1;
        return {ascent,
                descent,
                leading,
                ascent + descent + leading,
                std::max<int>(1, font->max_bounds.width)};
    }

    text_metrics font_t::measure_text(
        const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        auto *binding = linux::openlook::font_bindings
                            .object_from_handle(_id);
        if (!binding || !binding->metrics)
            return {};
        int direction = 0;
        int ascent = 0;
        int descent = 0;
        XCharStruct extent = {};
        XTextExtents(binding->metrics,
                     text.data(),
                     static_cast<int>(std::min<std::size_t>(
                         text.size(), INT_MAX)),
                     &direction,
                     &ascent,
                     &descent,
                     &extent);
        const int advance = extent.width;
        const int width = std::max(
            advance,
            static_cast<int>(extent.rbearing) - extent.lbearing);
        return {width, get_metrics().height, advance};
    }
} // namespace native
