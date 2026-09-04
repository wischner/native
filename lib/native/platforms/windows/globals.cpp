//
// Implements the Windows shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <windows.h>
#include <string>

#include <native.h>
#include <bindings.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace windows
{
    native::bindings<HWND, native::wnd *> wnd_bindings;
    native::bindings<uint32_t, win_font *> font_bindings;
    native::bindings<uint32_t, win_menu *> menu_bindings;
    std::unordered_map<native::code_edit *, wchar_t>
        code_edit_high_surrogates;

    native::rgba rgba_from_sys_color(int idx) {
        const COLORREF c = GetSysColor(idx);
        return native::rgba(static_cast<uint8_t>(GetRValue(c)),
                            static_cast<uint8_t>(GetGValue(c)),
                            static_cast<uint8_t>(GetBValue(c)),
                            255);
    }

    std::wstring utf8_to_wide(const std::string &text) {
        if (text.empty())
            return std::wstring();

        const int size = MultiByteToWideChar(
            CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (size <= 0)
            return std::wstring(text.begin(), text.end());

        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, text.c_str(), -1, &wide[0], size);
        if (!wide.empty() && wide.back() == L'\0')
            wide.pop_back();
        return wide;
    }

    std::string wide_to_utf8(const std::wstring &text) {
        if (text.empty())
            return std::string();

        const int size = WideCharToMultiByte(CP_UTF8,
                                             0,
                                             text.c_str(),
                                             -1,
                                             nullptr,
                                             0,
                                             nullptr,
                                             nullptr);
        if (size <= 0)
            return std::string(text.begin(), text.end());

        std::string utf8(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8,
                            0,
                            text.c_str(),
                            -1,
                            &utf8[0],
                            size,
                            nullptr,
                            nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
            utf8.pop_back();
        return utf8;
    }

    RECT to_rect(const native::rect &r) {
        RECT rc{static_cast<LONG>(r.p.x),
                static_cast<LONG>(r.p.y),
                static_cast<LONG>(r.x2()),
                static_cast<LONG>(r.y2())};
        return rc;
    }

    HWND hwnd_from_gpx(native::gpx &g) {
        auto *gw = dynamic_cast<native::gpx_wnd *>(&g);
        if (!gw)
            return nullptr;
        return wnd_bindings.handle_from_object(gw->window());
    }

    namespace
    {
        win_gpx *state_from_gpx(native::gpx &graphics) {
            auto *window_graphics =
                dynamic_cast<native::gpx_wnd *>(&graphics);
            return window_graphics
                       ? wnd_gpx_bindings.object_from_handle(
                             window_graphics->window())
                       : nullptr;
        }
    } // namespace

    scoped_gpx_dc::scoped_gpx_dc(native::gpx &graphics, HDC hdc)
        : _graphics(graphics)
        , _previous(nullptr)
        , _borrowed(hdc)
        , _saved_state(hdc ? SaveDC(hdc) : 0) {
        if (win_gpx *state = state_from_gpx(_graphics)) {
            _previous = state->hdc;
            state->hdc = hdc;
        }
    }

    scoped_gpx_dc::~scoped_gpx_dc() {
        if (win_gpx *state = state_from_gpx(_graphics))
            state->hdc = _previous;
        if (_borrowed && _saved_state)
            RestoreDC(_borrowed, _saved_state);
    }

    HDC acquire_gpx_dc(native::gpx &graphics) {
        if (win_gpx *state = state_from_gpx(graphics)) {
            if (state->hdc)
                return state->hdc;
        }
        const HWND hwnd = hwnd_from_gpx(graphics);
        return hwnd ? GetDC(hwnd) : nullptr;
    }

    void release_gpx_dc(native::gpx &graphics, HDC hdc) {
        if (!hdc)
            return;
        if (win_gpx *state = state_from_gpx(graphics)) {
            if (state->hdc == hdc)
                return;
        }
        if (const HWND hwnd = hwnd_from_gpx(graphics))
            ReleaseDC(hwnd, hdc);
    }

    HFONT control_font() {
        const native::font_t &font = native::font_t::stock(
            native::font_role::control);
        win_font *binding = font_bindings.object_from_handle(font.id());
        return binding && binding->hfont
                   ? binding->hfont
                   : static_cast<HFONT>(
                         GetStockObject(DEFAULT_GUI_FONT));
    }
} // namespace windows
