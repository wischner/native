#include <stdexcept>
#include <string>
#include <utility>

#include <windows.h>

#include <native.h>

#include "globals.h"

namespace native
{
    button::button(std::string text, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _text(std::move(text))
    {
    }

    button::button(const std::string &text, const point &pos, const size &dim)
        : button(text, pos.x, pos.y, dim.w, dim.h)
    {
    }

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &button::text() const
    {
        return _text;
    }

    button &button::set_text(const std::string &text)
    {
        _text = text;

        if (_created)
        {
            auto *h = win::button_bindings.from_a(const_cast<button *>(this));
            if (h && h->hwnd)
            {
                std::wstring wide = win::utf8_to_wide(_text);
                SetWindowTextW(h->hwnd, wide.c_str());
            }
        }

        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        wnd *p = parent();
        if (!p)
            throw std::runtime_error("Windows: button requires a parent window.");

        HWND parent_hwnd = win::wnd_bindings.from_b(p);
        if (!parent_hwnd)
            throw std::runtime_error("Windows: button parent is not created.");

        std::wstring text_w = win::utf8_to_wide(_text);
        HWND hwnd = CreateWindowExW(
            0,
            L"BUTTON",
            text_w.c_str(),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h,
            parent_hwnd,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr);

        if (!hwnd)
            throw std::runtime_error("Windows: Failed to create button.");

        auto *self = const_cast<button *>(this);
        win::wnd_bindings.register_pair(hwnd, self);

        auto *h = new win::winbutton();
        h->hwnd = hwnd;
        h->owner = self;
        win::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("Windows: Cannot show button before it is created.");

        auto *h = win::button_bindings.from_a(const_cast<button *>(this));
        if (!h || !h->hwnd)
            throw std::runtime_error("Windows: Missing HWND binding for button.");

        ShowWindow(h->hwnd, SW_SHOW);
        UpdateWindow(h->hwnd);
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = win::button_bindings.from_a(self);

        if (h)
        {
            if (h->hwnd)
            {
                DestroyWindow(h->hwnd);
                win::wnd_bindings.unregister_by_a(h->hwnd);
            }
            win::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native
