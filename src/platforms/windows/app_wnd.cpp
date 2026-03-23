#include <stdexcept>
#include <string>

#include <windows.h>

#include <native.h>

#include "globals.h"

namespace win
{
    constexpr wchar_t CLASS_NAME[] = L"native_window_class";

    LRESULT CALLBACK RoutedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static std::wstring utf8_to_wide(const std::string &text)
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

    void register_window_class()
    {
        static bool registered = false;
        if (registered)
            return;

        WNDCLASSW wc = {};
        wc.lpfnWndProc = RoutedWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClassW(&wc))
            throw std::runtime_error("Windows: Failed to register window class.");

        registered = true;
    }
} // namespace win

namespace native
{
    app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _title(std::move(title))
    {
    }

    app_wnd::app_wnd(const std::string &title, const point &pos, const size &dim)
        : app_wnd(title, pos.x, pos.y, dim.w, dim.h)
    {
    }

    app_wnd::app_wnd(const std::string &title, const rect &bounds)
        : app_wnd(title, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;

        if (_created)
        {
            HWND hwnd = win::wnd_bindings.from_b(this);
            if (hwnd)
            {
                std::wstring wide = win::utf8_to_wide(_title);
                SetWindowTextW(hwnd, wide.c_str());
            }
        }

        return *this;
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    void app_wnd::create() const
    {
        if (_created)
            return;

        win::register_window_class();

        std::wstring title_w = win::utf8_to_wide(_title);
        HWND hwnd = CreateWindowExW(
            0,
            win::CLASS_NAME,
            title_w.c_str(),
            WS_OVERLAPPEDWINDOW,
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h,
            nullptr, nullptr,
            GetModuleHandle(nullptr),
            const_cast<app_wnd *>(this));

        if (!hwnd)
            throw std::runtime_error("Windows: Failed to create window.");

        win::wnd_bindings.register_pair(hwnd, const_cast<app_wnd *>(this));
        _created = true;
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        if (!_created)
            throw std::runtime_error("Windows: Cannot show window before it is created.");

        HWND hwnd = win::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!hwnd)
            throw std::runtime_error("Windows: Missing HWND binding for app_wnd.");

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        HWND hwnd = win::wnd_bindings.from_b(self);

        if (auto *cache = win::wnd_gpx_bindings.from_a(self))
        {
            if (cache->pen)
                DeleteObject(cache->pen);
            if (cache->brush)
                DeleteObject(cache->brush);
            if (cache->font)
                DeleteObject(cache->font);
            delete cache;
            win::wnd_gpx_bindings.unregister_by_a(self);
        }

        if (hwnd)
        {
            DestroyWindow(hwnd);
            win::wnd_bindings.unregister_by_b(self);
        }

        _created = false;
    }
} // namespace native
