//
// Implements the macOS font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <cmath>
#include <native.h>
#include <native/font.h>
#include "../../portable_font.h"
#include "globals.h"

// font_t on macOS: the platform handle (mac_font) retains an NSFont and
// lives in mac::font_bindings, keyed by the font's opaque uint32_t id.

namespace
{
    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = mac::font_bindings.object_from_handle(id);
        if (f) {
            [f->ns_font release];
            delete f;
        }
        mac::font_bindings.unregister_by_handle(id);
    }

    uint32_t register_font(NSFont *nsfont) {
        [nsfont retain];
        auto *h = new mac::mac_font();
        h->ns_font = nsfont;
        uint32_t id = next_id();
        mac::font_bindings.register_pair(id, h);
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
        if (!initialized) {
            initialized = true;

            CGFloat sz = [NSFont systemFontSize];
            CGFloat sz_small = [NSFont smallSystemFontSize];

            auto init = [&](font_role r, NSFont *nsfont) {
                s[(int)r]._id = register_font(nsfont);
                NSString *family = [nsfont familyName];
                NSString *style = [nsfont displayName];
                const char *family_text =
                    family ? [family UTF8String] : nullptr;
                const char *style_text =
                    style ? [style UTF8String] : nullptr;
                s[(int)r]._spec.family =
                    family_text ? family_text : "";
                s[(int)r]._spec.style =
                    style_text ? style_text : "";
                s[(int)r]._spec.size = static_cast<int>(
                    std::lround([nsfont pointSize]));
                const NSFontTraitMask traits =
                    [[NSFontManager sharedFontManager]
                        traitsOfFont:nsfont];
                s[(int)r]._spec.italic =
                    (traits & NSItalicFontMask) != 0;
            };

            init(font_role::system, [NSFont systemFontOfSize:sz]);
            init(font_role::fixed,
                 [NSFont userFixedPitchFontOfSize:sz]);
            init(font_role::icon_label, [NSFont labelFontOfSize:0]);
            init(font_role::title, [NSFont titleBarFontOfSize:sz]);
            init(font_role::small,
                 [NSFont systemFontOfSize:sz_small]);
            init(font_role::control, [NSFont menuFontOfSize:0]);
            for (auto &font : s)
                font._spec.source = font_source::stock;
        }
        return s[(int)role];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        if (!_id)
            return {};
        auto *binding = mac::font_bindings.object_from_handle(_id);
        NSFont *font =
            binding && binding->ns_font
                ? binding->ns_font
                : [NSFont systemFontOfSize:[NSFont systemFontSize]];
        const int ascent = std::max(
            1, static_cast<int>(std::ceil([font ascender])));
        const int descent = std::max(
            1, static_cast<int>(std::ceil(-[font descender])));
        const int leading = std::max(
            1, static_cast<int>(std::ceil([font leading])));
        return {ascent,
                descent,
                leading,
                ascent + descent + leading,
                static_cast<int>(
                    std::ceil([font maximumAdvancement].width))};
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        auto *binding = mac::font_bindings.object_from_handle(_id);
        NSFont *font =
            binding && binding->ns_font
                ? binding->ns_font
                : [NSFont systemFontOfSize:[NSFont systemFontSize]];
        NSString *value = [NSString stringWithUTF8String:text.c_str()];
        if (!value)
            return {};
        NSDictionary *attributes = @{NSFontAttributeName : font};
        const NSSize measured = [value sizeWithAttributes:attributes];
        const int width = static_cast<int>(std::ceil(measured.width));
        return {width, get_metrics().height, width};
    }

} // namespace native
