//
// Implements the Haiku font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Font.h>
#include <Application.h>
#include <algorithm>
#include <cmath>

#include <native.h>
#include <native/font.h>
#include "../../portable_font.h"
#include "globals.h"

// font_t on Haiku: the platform handle (haiku_font) copies a BFont
// value and lives in haiku::font_bindings, keyed by the font's opaque
// uint32_t id.

namespace
{
    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = haiku::font_bindings.object_from_handle(id);
        if (f)
            delete f;
        haiku::font_bindings.unregister_by_handle(id);
    }

    uint32_t register_font(const BFont &bfont) {
        auto *h = new haiku::haiku_font();
        h->bfont = bfont;
        uint32_t id = next_id();
        haiku::font_bindings.register_pair(id, h);
        return id;
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
        if (!be_app) {
            static font_t fallback[6];
            static bool fallback_initialized = false;
            if (!fallback_initialized) {
                fallback_initialized = true;
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
            return fallback[(int)role];
        }

        if (!initialized) {
            initialized = true;

            s[(int)font_role::system]._id =
                register_font(*be_plain_font);
            s[(int)font_role::fixed]._id =
                register_font(*be_fixed_font);
            s[(int)font_role::icon_label]._id =
                register_font(*be_plain_font);
            s[(int)font_role::title]._id = register_font(*be_bold_font);
            s[(int)font_role::control]._id =
                register_font(*be_plain_font);

            BFont small = *be_plain_font;
            small.SetSize(
                std::max(8.0f, be_plain_font->Size() * 0.85f));
            s[(int)font_role::small]._id = register_font(small);
            for (auto &font : s) {
                font._spec.source = font_source::stock;
                auto *binding =
                    haiku::font_bindings.object_from_handle(font._id);
                if (!binding)
                    continue;
                font_family family = {};
                font_style style = {};
                binding->bfont.GetFamilyAndStyle(&family, &style);
                font._spec.family = family;
                font._spec.style = style;
                font._spec.size = static_cast<int>(
                    std::lround(binding->bfont.Size()));
            }
        }
        return s[(int)role];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        if (!_id)
            return {};
        auto *binding = haiku::font_bindings.object_from_handle(_id);
        const BFont *font = binding ? &binding->bfont : be_plain_font;
        if (!font)
            return {};
        font_height height = {};
        font->GetHeight(&height);
        const int ascent =
            std::max(1, static_cast<int>(std::ceil(height.ascent)));
        const int descent =
            std::max(1, static_cast<int>(std::ceil(height.descent)));
        const int leading =
            std::max(1, static_cast<int>(std::ceil(height.leading)));
        return {ascent,
                descent,
                leading,
                ascent + descent + leading,
                static_cast<int>(std::ceil(font->StringWidth("W")))};
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        auto *binding = haiku::font_bindings.object_from_handle(_id);
        const BFont *font = binding ? &binding->bfont : be_plain_font;
        const font_metrics metrics = get_metrics();
        if (!font)
            return {};
        const int width = static_cast<int>(std::ceil(font->StringWidth(
            text.c_str(), static_cast<int32>(text.size()))));
        return {width, metrics.height, width};
    }

} // namespace native
