//
// Implements the Windows font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <windows.h>
#include <algorithm>
#include <cstring>

#include <native.h>
#include <native/font.h>
#include "../../portable_font.h"
#include "globals.h"

// font_t on Windows: the platform handle (win_font) owns an HFONT and
// lives in windows::font_bindings, keyed by the font's opaque uint32_t
// id.

namespace
{
    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = windows::font_bindings.object_from_handle(id);
        if (f) {
            if (f->hfont)
                DeleteObject(f->hfont);
            delete f;
        }
        windows::font_bindings.unregister_by_handle(id);
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

            NONCLIENTMETRICSA ncm = {};
            ncm.cbSize = sizeof(ncm);
            SystemParametersInfoA(
                SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

            auto make = [](const LOGFONTA &lf) {
                font_t f;
                auto *h = new windows::win_font();
                h->hfont = CreateFontIndirectA(&lf);
                f._id = next_id();
                f._spec.family = lf.lfFaceName;
                f._spec.size =
                    (lf.lfHeight < 0) ? -lf.lfHeight : lf.lfHeight;
                f._spec.weight = lf.lfWeight;
                f._spec.italic = (lf.lfItalic != 0);
                windows::font_bindings.register_pair(f._id, h);
                return f;
            };

            s[(int)font_role::system] = make(ncm.lfMessageFont);
            LOGFONTA icon = ncm.lfMessageFont;
            SystemParametersInfoA(
                SPI_GETICONTITLELOGFONT, sizeof(icon), &icon, 0);
            s[(int)font_role::icon_label] = make(icon);
            s[(int)font_role::title] = make(ncm.lfCaptionFont);
            s[(int)font_role::small] = make(ncm.lfSmCaptionFont);
            s[(int)font_role::control] = make(ncm.lfMenuFont);

            LOGFONTA fixed = {};
            fixed.lfHeight = ncm.lfMessageFont.lfHeight;
            fixed.lfWeight = FW_NORMAL;
            fixed.lfCharSet = DEFAULT_CHARSET;
            fixed.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
            strcpy_s(fixed.lfFaceName, LF_FACESIZE, "Courier New");
            s[(int)font_role::fixed] = make(fixed);
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
        HDC dc = GetDC(nullptr);
        if (!dc)
            return {};
        auto *binding = windows::font_bindings.object_from_handle(_id);
        HFONT font =
            binding && binding->hfont
                ? binding->hfont
                : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ previous = SelectObject(dc, font);
        TEXTMETRICW metrics = {};
        const bool measured = GetTextMetricsW(dc, &metrics) != FALSE;
        SelectObject(dc, previous);
        ReleaseDC(nullptr, dc);
        if (!measured)
            return {};
        const int ascent = std::max<int>(1, metrics.tmAscent);
        const int descent = std::max<int>(1, metrics.tmDescent);
        const int leading =
            std::max<int>(1, metrics.tmExternalLeading);
        return {ascent,
                descent,
                leading,
                ascent + descent + leading,
                std::max<int>(1, metrics.tmMaxCharWidth)};
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        const font_metrics metrics = get_metrics();
        if (text.empty())
            return {0, metrics.height, 0};
        HDC dc = GetDC(nullptr);
        if (!dc)
            return {};
        auto *binding = windows::font_bindings.object_from_handle(_id);
        HFONT font =
            binding && binding->hfont
                ? binding->hfont
                : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ previous = SelectObject(dc, font);
        const std::wstring wide = windows::utf8_to_wide(text);
        SIZE extent = {};
        const bool measured =
            GetTextExtentPoint32W(dc,
                                  wide.data(),
                                  static_cast<int>(wide.size()),
                                  &extent) != FALSE;
        SelectObject(dc, previous);
        ReleaseDC(nullptr, dc);
        if (!measured)
            return {};
        return {extent.cx, metrics.height, extent.cx};
    }

} // namespace native
