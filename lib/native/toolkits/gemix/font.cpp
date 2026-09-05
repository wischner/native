//
// Implements the GEMix font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <array>

#include <native.h>
#include <native/font.h>

#include "../../portable_font.h"
#include "globals.h"
#include "stock_text.h"

namespace
{
    uint32_t next_font_id() {
        static uint32_t counter = 1;
        return counter++;
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
        if (detail::release_portable_font(_id))
            _id = 0;
    }

    const font_t &font_t::stock(font_role role) {
        static std::array<font_t, 6> fonts;
        static bool initialized = false;

        if (!initialized) {
            initialized = true;
            for (auto &font : fonts)
                font._id = next_font_id();

            fonts[static_cast<int>(font_role::system)]._spec.family =
                "GEM System";
            fonts[static_cast<int>(font_role::fixed)]._spec.family =
                "GEM Fixed";
            fonts[static_cast<int>(font_role::icon_label)]
                ._spec.family = "GEM Icon Label";
            fonts[static_cast<int>(font_role::title)]._spec.family =
                "GEM Title";
            fonts[static_cast<int>(font_role::small)]._spec.family =
                "GEM Small";
            fonts[static_cast<int>(font_role::control)]._spec.family =
                "GEM Control";
            for (auto &font : fonts) {
                font._spec.source = font_source::stock;
                font._spec.size = linux::gemix::ensure_runtime()
                    ? linux::gemix::runtime.char_h
                    : 8;
            }
        }

        return fonts[static_cast<int>(role)];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        if (!_id)
            return {};
        if (!linux::gemix::ensure_runtime())
            return {};
        const int height = std::max<int>(
            3, linux::gemix::runtime.char_h);
        return {height - 2,
                1,
                1,
                height,
                std::max<int>(1, linux::gemix::runtime.char_w)};
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        const font_metrics metrics = get_metrics();
        const int width =
            static_cast<int>(linux::gemix::stock_text(text).size()) * metrics.max_advance;
        return {width, metrics.height, width};
    }
} // namespace native
