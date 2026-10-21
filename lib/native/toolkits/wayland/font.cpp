//
// Uses Pango stock metrics and the shared portable TrueType font path.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../portable_font.h"
#include <array>
#include <algorithm>
#include <pango/pangocairo.h>
#include <utility>
namespace
{
    PangoLayout *layout(const native::font_spec &spec,
                        const std::string &text) {
        auto *context = pango_font_map_create_context(
            pango_cairo_font_map_get_default());
        auto *result = pango_layout_new(context);
        g_object_unref(context);
        auto *desc = pango_font_description_new();
        pango_font_description_set_family(desc, spec.family.c_str());
        pango_font_description_set_absolute_size(
            desc, spec.size * PANGO_SCALE);
        pango_layout_set_font_description(result, desc);
        pango_font_description_free(desc);
        pango_layout_set_text(result, text.c_str(), text.size());
        return result;
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
        std::swap(_id, other._id);
        std::swap(_spec, other._spec);
        return *this;
    }
    font_t::~font_t() {
        detail::release_portable_font(_id);
    }
    const font_t &font_t::stock(font_role role) {
        static std::array<font_t, 6> fonts;
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            char *name = nullptr;
            if (auto *settings = gtk_settings_get_default())
                g_object_get(settings, "gtk-font-name", &name, nullptr);
            auto *description = pango_font_description_from_string(
                name ? name : "Sans 10");
            for (unsigned i = 0; i < fonts.size(); ++i) {
                fonts[i]._id = i + 1;
                fonts[i]._spec.source = font_source::stock;
                fonts[i]._spec.family =
                    pango_font_description_get_family(description);
                fonts[i]._spec.size =
                    pango_font_description_get_size(description) * 96 /
                    (72 * PANGO_SCALE);
            }
            fonts[static_cast<int>(font_role::fixed)]._spec.family =
                "Monospace";
            fonts[static_cast<int>(font_role::small)]._spec.size -= 2;
            pango_font_description_free(description);
            g_free(name);
        }
        return fonts[static_cast<int>(role)];
    }
    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        if (!_id)
            return {};
        auto *l = layout(_spec, "M");
        int w = 0, h = 0;
        pango_layout_get_pixel_size(l, &w, &h);
        const int ascent = pango_layout_get_baseline(l) / PANGO_SCALE;
        g_object_unref(l);
        return {ascent, std::max(1, h - ascent), 1, h + 1, w};
    }
    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        auto *l = layout(_spec, text);
        int w = 0, h = 0;
        pango_layout_get_pixel_size(l, &w, &h);
        g_object_unref(l);
        return {w, h, w};
    }
} // namespace native
