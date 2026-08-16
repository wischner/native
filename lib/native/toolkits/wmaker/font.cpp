//
// Implements Window Maker stock fonts with WINGs font resources while
// delegating file and memory fonts to the shared portable engine.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <climits>
#include <cstdint>
#include <utility>

#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/font.h>

#include "../../portable_font.h"
#include "globals.h"

namespace
{
    std::uint32_t next_id() {
        static std::uint32_t current = 0;
        return ++current;
    }

    std::uint32_t register_font(WMFont *font, bool owned) {
        auto *binding = new linux::wmaker::native_font;
        binding->font = font;
        binding->owned = owned;
        const std::uint32_t id = next_id();
        linux::wmaker::font_bindings.register_pair(id, binding);
        return id;
    }

    void release(std::uint32_t id) {
        auto *binding = linux::wmaker::font_bindings
                            .object_from_handle(id);
        if (binding) {
            if (binding->owned && binding->font)
                WMReleaseFont(binding->font);
            delete binding;
        }
        linux::wmaker::font_bindings.unregister_by_handle(id);
    }

    WMFont *fallback_font(int size) {
        WMFont *font = WMSystemFontOfSize(
            linux::wmaker::screen, size);
        if (!font) {
            font = WMCreateFont(linux::wmaker::screen,
                                "sans serif:pixelsize=12");
        }
        return font;
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
        if (!linux::wmaker::screen) {
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
                    for (font_t &font : fallback)
                        font._spec.source = font_source::stock;
                    break;
                }
            }
            return fallback[static_cast<int>(role)];
        }

        if (!ready) {
            ready = true;
            WMFont *system = WMDefaultSystemFont(
                linux::wmaker::screen);
            WMFont *fixed = WMCreateFont(
                linux::wmaker::screen,
                "monospace:pixelsize=12");
            WMFont *title = WMDefaultBoldSystemFont(
                linux::wmaker::screen);
            WMFont *small = WMSystemFontOfSize(
                linux::wmaker::screen, 10);
            if (!system)
                system = fallback_font(12);
            if (!fixed)
                fixed = fallback_font(12);
            if (!title)
                title = fallback_font(13);
            if (!small)
                small = fallback_font(10);

            WMFont *selected[] = {
                system, fixed, WMRetainFont(system), title, small,
                WMRetainFont(system)};
            const char *families[] = {
                "WINGs system", "WINGs monospace", "WINGs system",
                "WINGs bold", "WINGs small", "WINGs system"};
            for (int index = 0; index < 6; ++index) {
                fonts[index]._id = register_font(
                    selected[index], true);
                fonts[index]._spec.family = families[index];
                fonts[index]._spec.source = font_source::stock;
            }
        }
        return fonts[static_cast<int>(role)];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        auto *binding = linux::wmaker::font_bindings
                            .object_from_handle(_id);
        if (!binding || !binding->font)
            return {};
        auto *font = reinterpret_cast<W_Font *>(binding->font);
        const int ascent = std::max(1, static_cast<int>(font->y));
        const int height = std::max(
            ascent, static_cast<int>(WMFontHeight(binding->font)));
        const int descent = std::max(1, height - ascent);
        const int maximum = std::max(
            1,
            WMWidthOfString(binding->font,
                            "M",
                            1));
        return {ascent, descent, 0, height, maximum};
    }

    text_metrics font_t::measure_text(
        const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        auto *binding = linux::wmaker::font_bindings
                            .object_from_handle(_id);
        if (!binding || !binding->font)
            return {};
        const int length = static_cast<int>(std::min<std::size_t>(
            text.size(), INT_MAX));
        const int width = WMWidthOfString(
            binding->font, text.c_str(), length);
        return {width, get_metrics().height, width};
    }
} // namespace native
