//
// Implements status_bar with the Win32 common-controls status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <commctrl.h>

#include <native.h>

#include "../../status_bar_peer.h"
#include "globals.h"

namespace
{
    class windows_status_bar_peer final
        : public native::detail::status_bar_peer
    {
    public:
        ~windows_status_bar_peer() override {
            destroy();
        }

        bool update(native::status_bar &bar,
                    const native::rect &bounds) override {
            native::wnd *owner = bar.get_owner();
            HWND parent = owner
                              ? windows::wnd_bindings
                                    .handle_from_object(owner)
                              : nullptr;
            if (!parent || !IsWindow(parent)) {
                destroy();
                return false;
            }

            std::vector<native::status_bar_part> parts = bar.get_parts();
            if (parts.empty())
                parts.push_back({bar.get_text(), 0});
            if (parts.size() > 256) {
                destroy();
                return false;
            }

            if (_window &&
                (!IsWindow(_window) || GetParent(_window) != parent)) {
                destroy();
            }
            if (!_window) {
                INITCOMMONCONTROLSEX controls{
                    sizeof(controls), ICC_BAR_CLASSES};
                InitCommonControlsEx(&controls);
                DWORD style = WS_CHILD | CCS_NOPARENTALIGN |
                    CCS_NORESIZE;
                if (dynamic_cast<native::app_wnd *>(owner) &&
                    bounds.x2() == owner->get_dimensions().w) {
                    style |= SBARS_SIZEGRIP;
                }
                _window = CreateWindowExW(
                    0,
                    STATUSCLASSNAMEW,
                    L"",
                    style,
                    bounds.x1(),
                    bounds.y1(),
                    bounds.w(),
                    bounds.h(),
                    parent,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr);
                if (!_window)
                    return false;
                SendMessageW(_window,
                             WM_SETFONT,
                             reinterpret_cast<WPARAM>(
                                 windows::control_font()),
                             TRUE);
            }

            RECT actual{};
            GetWindowRect(_window, &actual);
            MapWindowPoints(nullptr, parent,
                reinterpret_cast<POINT *>(&actual), 2);
            if (actual.left != bounds.x1() ||
                actual.top != bounds.y1() ||
                actual.right != bounds.x2() ||
                actual.bottom != bounds.y2()) {
                SetWindowPos(_window, nullptr,
                    bounds.x1(), bounds.y1(), bounds.w(), bounds.h(),
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            // Edge chrome is above document children even with absolute
            // layouts that extend outside the reserved client rectangle.
            // Sibling clipping also prevents their own native repaint from
            // painting through this higher-z-order status bar.
            for (HWND child = GetWindow(parent, GW_CHILD); child;
                 child = GetWindow(child, GW_HWNDNEXT)) {
                const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
                if ((style & WS_CLIPSIBLINGS) == 0)
                    SetWindowLongPtrW(child, GWL_STYLE,
                                      style | WS_CLIPSIBLINGS);
            }
            if (GetWindow(parent, GW_CHILD) != _window)
                SetWindowPos(_window, HWND_TOP, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            const bool changed = _width != bounds.w() ||
                _parts.size() != parts.size() ||
                !std::equal(_parts.begin(), _parts.end(), parts.begin(),
                    [](const auto &left, const auto &right) {
                        return left.text == right.text &&
                               left.width == right.width;
                    });
            if (changed) {
                synchronize_parts(parts, bounds.w());
                _parts = parts;
                _width = bounds.w();
            }
            ShowWindow(_window,
                       bar.get_visible() ? SW_SHOWNA : SW_HIDE);
            return true;
        }

    private:
        HWND _window = nullptr;
        std::vector<native::status_bar_part> _parts;
        int _width = -1;

        void destroy() {
            if (_window && IsWindow(_window))
                DestroyWindow(_window);
            _window = nullptr;
            _parts.clear();
            _width = -1;
        }

        void synchronize_parts(
            const std::vector<native::status_bar_part> &parts,
            int total_width) {
            int fixed = 0;
            int flexible = 0;
            for (const auto &part : parts) {
                if (part.width > 0)
                    fixed += part.width;
                else
                    ++flexible;
            }
            int remaining = std::max(0, total_width-fixed);
            int flexible_left = flexible;
            int edge = 0;
            std::vector<int> edges;
            edges.reserve(parts.size());
            for (std::size_t index = 0;
                 index < parts.size();
                 ++index) {
                int width = parts[index].width;
                if (!width && flexible_left > 0) {
                    width = remaining/flexible_left;
                    remaining -= width;
                    --flexible_left;
                }
                edge = std::min(total_width, edge+std::max(0, width));
                edges.push_back(index+1 == parts.size() ? -1 : edge);
            }
            SendMessageW(_window,
                         SB_SETPARTS,
                         static_cast<WPARAM>(edges.size()),
                         reinterpret_cast<LPARAM>(edges.data()));
            for (std::size_t index = 0;
                 index < parts.size();
                 ++index) {
                const std::wstring text =
                    windows::utf8_to_wide(parts[index].text);
                SendMessageW(
                    _window,
                    SB_SETTEXTW,
                    static_cast<WPARAM>(index),
                    reinterpret_cast<LPARAM>(text.c_str()));
            }
        }
    };
} // namespace

namespace native::detail
{
    std::unique_ptr<status_bar_peer> create_status_bar_peer() {
        return std::make_unique<windows_status_bar_peer>();
    }
} // namespace native::detail
