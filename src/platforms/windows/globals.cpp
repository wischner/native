#include <windows.h>
#include <string>

#include <native.h>
#include <bindings.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace win
{
    native::bindings<HWND, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, wingpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, winfont *> font_bindings;
    native::bindings<uint32_t, winmenu *> menu_bindings;
    native::bindings<native::button *, winbutton *> button_bindings;

    native::rgba rgba_from_sys_color(int idx)
    {
        const COLORREF c = GetSysColor(idx);
        return native::rgba(
            static_cast<uint8_t>(GetRValue(c)),
            static_cast<uint8_t>(GetGValue(c)),
            static_cast<uint8_t>(GetBValue(c)),
            255);
    }

    std::wstring utf8_to_wide(const std::string &text)
    {
        if (text.empty())
            return std::wstring();

        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (size <= 0)
            return std::wstring(text.begin(), text.end());

        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], size);
        if (!wide.empty() && wide.back() == L'\0')
            wide.pop_back();
        return wide;
    }

    RECT to_rect(const native::rect &r)
    {
        RECT rc{
            static_cast<LONG>(r.p.x),
            static_cast<LONG>(r.p.y),
            static_cast<LONG>(r.x2() + 1),
            static_cast<LONG>(r.y2() + 1)
        };
        return rc;
    }

    HWND hwnd_from_gpx(native::gpx &g)
    {
        auto *gw = dynamic_cast<native::gpx_wnd *>(&g);
        if (!gw)
            return nullptr;
        return wnd_bindings.from_b(gw->window());
    }
}
