#include <native.h>
#include <bindings.h>
#include <windows.h>
#include <iostream>

namespace win {
    extern native::bindings<HWND, native::wnd*> wnd_bindings;

    // Window class name
    constexpr wchar_t CLASS_NAME[] = L"native_window_class";

    // Dummy WndProc just for create/show (message loop handled elsewhere)
    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // Ensure class registered only once
    void register_window_class()
    {
        static bool registered = false;
        if (registered) return;

        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

        if (!RegisterClassW(&wc))
        {
            std::cerr << "Windows: Failed to register window class.\n";
        }

        registered = true;
    }
}

namespace native {

void app_wnd::create() const
{
    win::register_window_class();

    HWND hwnd = CreateWindowExW(
        0,
        win::CLASS_NAME,
        std::wstring(title().begin(), title().end()).c_str(), // convert to UTF-16
        WS_OVERLAPPEDWINDOW,
        x(), y(), width(), height(),
        nullptr, nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (!hwnd)
    {
        std::cerr << "Windows: Failed to create window.\n";
        return;
    }

    // Register in binding table
    win::wnd_bindings.register_pair(hwnd, const_cast<app_wnd*>(this));
}

void app_wnd::show() const
{
    HWND hwnd = win::wnd_bindings.from_b(const_cast<app_wnd*>(this));
    if (!hwnd)
    {
        std::cerr << "Windows: Can't show window, not created.\n";
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

} // namespace native
