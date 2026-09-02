//
// Implements the Windows window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <windows.h>
#include <windowsx.h>

#include <native.h>
#include <native/wnd.h>

#include "../../control_render_access.h"
#include "gpx_wnd.h"
#include "globals.h"

namespace windows
{
    static void app_window_frame_size(HWND hwnd,
                                      LONG &width,
                                      LONG &height) {
        RECT client = {0, 0, width, height};
        const DWORD style = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd, GWL_STYLE));
        const DWORD extended_style = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        if (AdjustWindowRectEx(
                &client,
                style,
                GetMenu(hwnd) != nullptr,
                extended_style)) {
            width = client.right - client.left;
            height = client.bottom - client.top;
        }
    }

    static bool is_user_input_message(UINT message) {
        return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
               (message >= WM_MOUSEFIRST &&
                message <= WM_MOUSELAST) ||
               message == WM_COMMAND || message == WM_SYSCOMMAND;
    }

    static native::mouse_button button_from_msg(UINT message,
                                                WPARAM wparam) {
        switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            return native::mouse_button::left;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return native::mouse_button::right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return native::mouse_button::middle;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
            return (HIWORD(wparam) == XBUTTON1)
                       ? native::mouse_button::x1
                       : native::mouse_button::x2;
        default:
            return native::mouse_button::none;
        }
    }

    LRESULT CALLBACK routed_wnd_proc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
        if (message == WM_NCCREATE) {
            auto *create = reinterpret_cast<CREATESTRUCT *>(lparam);
            auto *native_wnd =
                reinterpret_cast<native::wnd *>(create->lpCreateParams);
            if (native_wnd) {
                SetWindowLongPtr(
                    hwnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(native_wnd));
                wnd_bindings.register_pair(hwnd, native_wnd);
            }
        }

        native::wnd *wnd = wnd_bindings.object_from_handle(hwnd);
        if (!wnd) {
            wnd = reinterpret_cast<native::wnd *>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (wnd)
                wnd_bindings.register_pair(hwnd, wnd);
        }

        if (!wnd)
            return DefWindowProcW(hwnd, message, wparam, lparam);

        if (!wnd->get_input_enabled() &&
            is_user_input_message(message)) {
            return 0;
        }

        switch (message) {
        case WM_MOVE: {
            native::point position(GET_X_LPARAM(lparam),
                                   GET_Y_LPARAM(lparam));
            if (dynamic_cast<native::app_wnd *>(wnd)) {
                RECT bounds = {};
                if (GetWindowRect(hwnd, &bounds)) {
                    position = native::point(
                        static_cast<native::coord>(bounds.left),
                        static_cast<native::coord>(bounds.top));
                }
            }
            wnd->on_native_move(position);
            break;
        }

        case WM_SIZE: {
            native::size s(LOWORD(lparam), HIWORD(lparam));
            wnd->on_native_resize(s);
            break;
        }

        case WM_MOUSEMOVE:
            {
                POINT screen{
                    GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(hwnd, &screen);
                wnd->on_native_mouse_move(
                    native::point(GET_X_LPARAM(lparam),
                                  GET_Y_LPARAM(lparam)),
                    native::point(screen.x, screen.y));
            }
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP: {
            const native::mouse_button btn =
                button_from_msg(message, wparam);
            const bool is_press = message == WM_LBUTTONDOWN ||
                                  message == WM_LBUTTONDBLCLK ||
                                  message == WM_RBUTTONDOWN ||
                                  message == WM_MBUTTONDOWN ||
                                  message == WM_XBUTTONDOWN;
            const native::mouse_action act =
                is_press ? native::mouse_action::press
                         : native::mouse_action::release;

            if (btn != native::mouse_button::none) {
                if (is_press) {
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                } else if (GetCapture() == hwnd) {
                    ReleaseCapture();
                }
                native::mouse_event me(
                    btn,
                    act,
                    native::point(GET_X_LPARAM(lparam),
                                  GET_Y_LPARAM(lparam)));
                wnd->on_native_mouse_click(me);
                if (message == WM_LBUTTONDBLCLK) {
                    if (auto *icons =
                            dynamic_cast<native::icon_view *>(wnd)) {
                        icons->on_native_activate(icons->item_at(
                            native::point(GET_X_LPARAM(lparam),
                                          GET_Y_LPARAM(lparam))));
                    }
                }
            }
            break;
        }

        case WM_SETFOCUS:
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(wnd))
                accordion->on_native_focus(true);
            if (auto *icons = dynamic_cast<native::icon_view *>(wnd))
                icons->on_native_focus(true);
            if (auto *editor = dynamic_cast<native::code_edit *>(wnd))
                editor->on_native_focus(true);
            break;

        case WM_KILLFOCUS:
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(wnd))
                accordion->on_native_focus(false);
            if (auto *icons = dynamic_cast<native::icon_view *>(wnd))
                icons->on_native_focus(false);
            if (auto *editor = dynamic_cast<native::code_edit *>(wnd))
                editor->on_native_focus(false);
            break;

        case WM_KEYDOWN:
            if (auto *editor = dynamic_cast<native::code_edit *>(wnd)) {
                const bool extend =
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool command =
                    (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (command) {
                    native::code_edit_key key;
                    bool handled = true;
                    if (wparam == 'A')
                        key = native::code_edit_key::select_all;
                    else if (wparam == 'C')
                        key = native::code_edit_key::copy;
                    else if (wparam == 'X')
                        key = native::code_edit_key::cut;
                    else if (wparam == 'V')
                        key = native::code_edit_key::paste;
                    else if (wparam == 'Z')
                        key = extend ? native::code_edit_key::redo
                                     : native::code_edit_key::undo;
                    else
                        handled = false;
                    if (handled) {
                        editor->on_native_key(key);
                        return 0;
                    }
                }
                native::code_edit_key key;
                bool handled = true;
                switch (wparam) {
                case VK_LEFT:
                    key = native::code_edit_key::left;
                    break;
                case VK_RIGHT:
                    key = native::code_edit_key::right;
                    break;
                case VK_UP:
                    key = native::code_edit_key::up;
                    break;
                case VK_DOWN:
                    key = native::code_edit_key::down;
                    break;
                case VK_HOME:
                    key = native::code_edit_key::home;
                    break;
                case VK_END:
                    key = native::code_edit_key::end;
                    break;
                case VK_PRIOR:
                    key = native::code_edit_key::page_up;
                    break;
                case VK_NEXT:
                    key = native::code_edit_key::page_down;
                    break;
                case VK_BACK:
                    key = native::code_edit_key::backspace;
                    break;
                case VK_DELETE:
                    key = native::code_edit_key::delete_forward;
                    break;
                case VK_RETURN:
                    key = native::code_edit_key::enter;
                    break;
                case VK_TAB:
                    key = native::code_edit_key::tab;
                    break;
                case VK_ESCAPE:
                    key = native::code_edit_key::escape;
                    break;
                default:
                    handled = false;
                    break;
                }
                if (handled) {
                    editor->on_native_key(key, extend);
                    return 0;
                }
            }
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(wnd)) {
                switch (wparam) {
                case VK_UP:
                    accordion->on_native_navigation(
                        native::accordion_navigation::previous);
                    return 0;
                case VK_DOWN:
                    accordion->on_native_navigation(
                        native::accordion_navigation::next);
                    return 0;
                case VK_HOME:
                    accordion->on_native_navigation(
                        native::accordion_navigation::first);
                    return 0;
                case VK_END:
                    accordion->on_native_navigation(
                        native::accordion_navigation::last);
                    return 0;
                case VK_RETURN:
                case VK_SPACE:
                    accordion->on_native_navigation(
                        native::accordion_navigation::toggle);
                    return 0;
                }
            }
            break;

        case WM_CHAR:
            if (auto *editor = dynamic_cast<native::code_edit *>(wnd)) {
                if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
                    wparam >= 0x20 && wparam != 0x7f) {
                    const wchar_t unit =
                        static_cast<wchar_t>(wparam);
                    if (unit >= 0xd800 && unit <= 0xdbff) {
                        code_edit_high_surrogates[editor] = unit;
                        return 0;
                    }
                    std::wstring value;
                    const auto pending =
                        code_edit_high_surrogates.find(editor);
                    if (unit >= 0xdc00 && unit <= 0xdfff) {
                        if (pending ==
                            code_edit_high_surrogates.end()) {
                            return 0;
                        }
                        value.push_back(pending->second);
                        value.push_back(unit);
                    } else {
                        value.push_back(unit);
                    }
                    code_edit_high_surrogates.erase(editor);
                    const std::string utf8 = wide_to_utf8(value);
                    if (!utf8.empty())
                        editor->on_native_text_input(utf8);
                }
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
#ifdef WM_MOUSEHWHEEL
        case WM_MOUSEHWHEEL:
#endif
        {
            POINT screen_pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &screen_pt);

            native::wheel_direction wdir =
                native::wheel_direction::vertical;
#ifdef WM_MOUSEHWHEEL
            if (message == WM_MOUSEHWHEEL)
                wdir = native::wheel_direction::horizontal;
#endif
            native::mouse_wheel_event wheel(
                native::point(screen_pt.x, screen_pt.y),
                static_cast<native::coord>(
                    GET_WHEEL_DELTA_WPARAM(wparam)),
                wdir);
            wnd->on_native_mouse_wheel(wheel);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            native::rect r(static_cast<native::coord>(ps.rcPaint.left),
                           static_cast<native::coord>(ps.rcPaint.top),
                           static_cast<native::dim>(ps.rcPaint.right -
                                                    ps.rcPaint.left),
                           static_cast<native::dim>(ps.rcPaint.bottom -
                                                    ps.rcPaint.top));

            auto &g = wnd->get_gpx().set_clip(r);
            g.clear(native::rgba(255, 255, 255, 255));
            native::wnd_paint_event e{r, g};
            wnd->on_native_paint(e);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM:
            if (lparam != 0) {
                auto *drawing = reinterpret_cast<DRAWITEMSTRUCT *>(
                    lparam);
                auto *child = windows::wnd_bindings.object_from_handle(
                    drawing->hwndItem);
                if (!child)
                    break;
                auto *button = dynamic_cast<native::button *>(child);
                auto *check = dynamic_cast<native::check *>(child);
                auto *radio = dynamic_cast<native::radio *>(child);
                if (!button && !check && !radio)
                    break;

                native::rect bounds(
                    static_cast<native::coord>(drawing->rcItem.left),
                    static_cast<native::coord>(drawing->rcItem.top),
                    static_cast<native::dim>(
                        drawing->rcItem.right - drawing->rcItem.left),
                    static_cast<native::dim>(
                        drawing->rcItem.bottom - drawing->rcItem.top));
                native::gpx &graphics =
                    child->get_gpx().set_clip(bounds);
                windows::scoped_gpx_dc custom_draw_context(
                    graphics, drawing->hDC);
                auto appearance = native::theme::create(graphics);
                native::theme::state state;
                state.disabled =
                    (drawing->itemState & ODS_DISABLED) != 0;
                state.hot = (drawing->itemState & ODS_HOTLIGHT) != 0;
                state.pressed =
                    (drawing->itemState & ODS_SELECTED) != 0;
                state.focused =
                    (drawing->itemState & ODS_FOCUS) != 0;
                if (button) {
                    native::detail::control_render_access::draw(
                        *button,
                        graphics,
                        *appearance,
                        bounds,
                        state);
                } else if (check) {
                    native::detail::control_render_access::draw(
                        *check,
                        graphics,
                        *appearance,
                        bounds,
                        state);
                } else {
                    native::detail::control_render_access::draw(
                        *radio,
                        graphics,
                        *appearance,
                        bounds,
                        state);
                }
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (lparam != 0) {
                HWND control = reinterpret_cast<HWND>(lparam);
                if (auto *child =
                        windows::wnd_bindings.object_from_handle(
                            control)) {
                    if (auto *btn =
                            dynamic_cast<native::button *>(child)) {
                        btn->on_native_click();
                        return 0;
                    }
                    if (auto *check =
                            dynamic_cast<native::check *>(child)) {
                        if (HIWORD(wparam) == BN_CLICKED) {
                            check->on_native_checked(
                                !check->get_checked());
                            SendMessageW(
                                control,
                                BM_SETCHECK,
                                check->get_checked() ? BST_CHECKED
                                                     : BST_UNCHECKED,
                                0);
                        }
                        return 0;
                    }
                    if (auto *radio =
                            dynamic_cast<native::radio *>(child)) {
                        if (HIWORD(wparam) == BN_CLICKED)
                            radio->on_native_selected();
                        return 0;
                    }
                    if (auto *list =
                            dynamic_cast<native::list *>(child)) {
                        if (HIWORD(wparam) == LBN_SELCHANGE) {
                            list->on_native_selection(
                                static_cast<int>(SendMessageW(
                                    control, LB_GETCURSEL, 0, 0)));
                        }
                        return 0;
                    }
                    if (auto *combo =
                            dynamic_cast<native::combo_box *>(child)) {
                        const int notification = HIWORD(wparam);
                        if (notification == CBN_SELCHANGE) {
                            combo->on_native_selection(
                                static_cast<int>(SendMessageW(
                                    control, CB_GETCURSEL, 0, 0)));
                        } else if (notification == CBN_EDITCHANGE) {
                            const int length = GetWindowTextLengthW(control);
                            std::wstring text(static_cast<std::size_t>(length+1),
                                              L'\0');
                            if (length > 0)
                                GetWindowTextW(control, text.data(), length+1);
                            text.resize(static_cast<std::size_t>(length));
                            combo->on_native_text(
                                windows::wide_to_utf8(text));
                        } else if (notification == CBN_DROPDOWN) {
                            combo->on_native_drop_down(true);
                        } else if (notification == CBN_CLOSEUP) {
                            combo->on_native_drop_down(false);
                        }
                        return 0;
                    }
                    if (auto *editor =
                            dynamic_cast<native::text_edit *>(child)) {
                        if (HIWORD(wparam) == EN_CHANGE)
                            windows::handle_text_edit_change(editor);
                        return 0;
                    }
                }
            } else if (HIWORD(wparam) == 0) {
                // Menu item click (lparam == 0).
                if (auto *aw = dynamic_cast<native::app_wnd *>(wnd)) {
                    aw->on_native_menu(
                        static_cast<int>(LOWORD(wparam)));
                    return 0;
                }
            }
            return 0;

        case WM_NOTIFY:
            if (lparam != 0) {
                auto *notification =
                    reinterpret_cast<NMHDR *>(lparam);
                auto *child = windows::wnd_bindings.object_from_handle(
                    notification->hwndFrom);
                if (!child) {
                    child = windows::wnd_bindings.object_from_handle(
                        GetParent(notification->hwndFrom));
                }
                if (auto *table =
                        dynamic_cast<native::table_view *>(child)) {
                    return windows::handle_table_notify(
                        table, notification);
                }
                if (auto *tree =
                        dynamic_cast<native::tree_view *>(child)) {
                    return windows::handle_tree_notify(
                        tree, notification);
                }
                if (auto *tabs =
                        dynamic_cast<native::tab_view *>(child)) {
                    if (notification->code == TCN_SELCHANGE) {
                        const int selected = TabCtrl_GetCurSel(
                            notification->hwndFrom);
                        if (selected >= 0 &&
                            !tabs->get_item(
                                static_cast<std::size_t>(selected))
                                 .get_enabled()) {
                            TabCtrl_SetCurSel(
                                notification->hwndFrom,
                                tabs->get_selected_index());
                        } else if (selected >= 0) {
                            tabs->on_native_selection(selected);
                        }
                    }
                    return 0;
                }
                if (auto *icons =
                        dynamic_cast<native::icon_view *>(child)) {
                    auto *binding = windows::icon_view_bindings
                                        .object_from_handle(icons);
                    if (!binding)
                        return 0;
                    if (notification->code == NM_CUSTOMDRAW) {
                        auto *drawing =
                            reinterpret_cast<NMLVCUSTOMDRAW *>(
                                notification);
                        native::gpx &graphics =
                            icons->get_gpx();
                        windows::scoped_gpx_dc custom_draw_context(
                            graphics, drawing->nmcd.hdc);
                        auto appearance = native::theme::create(graphics);
                        if (drawing->nmcd.dwDrawStage == CDDS_PREPAINT) {
                            RECT client{};
                            GetClientRect(binding->hwnd, &client);
                            const native::rect bounds(
                                0,
                                0,
                                static_cast<native::dim>(client.right),
                                static_cast<native::dim>(client.bottom));
                            graphics.set_clip(bounds);
                            native::theme::state state;
                            state.focused = GetFocus() == binding->hwnd;
                            native::detail::control_render_access::
                                draw_icon_background(
                                    *icons,
                                    graphics,
                                    *appearance,
                                    bounds,
                                    state);
                            return CDRF_NOTIFYITEMDRAW;
                        }
                        if (drawing->nmcd.dwDrawStage ==
                            CDDS_ITEMPREPAINT) {
                            const std::size_t index =
                                static_cast<std::size_t>(
                                    drawing->nmcd.dwItemSpec);
                            if (index >= icons->get_items().size())
                                return CDRF_DODEFAULT;
                            RECT item{};
                            if (!ListView_GetItemRect(
                                    binding->hwnd,
                                    static_cast<int>(index),
                                    &item,
                                    LVIR_BOUNDS)) {
                                return CDRF_DODEFAULT;
                            }
                            const native::rect bounds(
                                static_cast<native::coord>(item.left),
                                static_cast<native::coord>(item.top),
                                static_cast<native::dim>(
                                    item.right - item.left),
                                static_cast<native::dim>(
                                    item.bottom - item.top));
                            graphics.set_clip(bounds);
                            native::theme::state state;
                            state.selected =
                                (drawing->nmcd.uItemState &
                                 CDIS_SELECTED) != 0;
                            state.hot =
                                (drawing->nmcd.uItemState & CDIS_HOT) !=
                                0;
                            state.focused = state.selected &&
                                GetFocus() == binding->hwnd;
                            state.disabled =
                                !icons->get_items()[index].enabled;
                            native::detail::control_render_access::
                                draw_icon_item(
                                    *icons,
                                    graphics,
                                    *appearance,
                                    index,
                                    icons->get_items()[index],
                                    bounds,
                                    state);
                            return CDRF_SKIPDEFAULT;
                        }
                        return CDRF_DODEFAULT;
                    }
                    if (binding->suppress)
                        return 0;
                    if (notification->code == LVN_ITEMCHANGED) {
                        const int selected = ListView_GetNextItem(
                            notification->hwndFrom,
                            -1,
                            LVNI_SELECTED);
                        if (selected >= 0 &&
                            !icons->get_items()[selected].enabled) {
                            binding->suppress = true;
                            ListView_SetItemState(
                                notification->hwndFrom,
                                -1,
                                0,
                                LVIS_SELECTED | LVIS_FOCUSED);
                            const int previous =
                                icons->get_selected_index();
                            if (previous >= 0) {
                                ListView_SetItemState(
                                    notification->hwndFrom,
                                    previous,
                                    LVIS_SELECTED | LVIS_FOCUSED,
                                    LVIS_SELECTED | LVIS_FOCUSED);
                            }
                            binding->suppress = false;
                        } else {
                            icons->on_native_selection(selected);
                        }
                    } else if (notification->code == NM_DBLCLK ||
                               notification->code == NM_RETURN) {
                        icons->on_native_activate(
                            icons->get_selected_index());
                    }
                    return 0;
                }
            }
            break;

        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC: {
            HWND control = reinterpret_cast<HWND>(lparam);
            native::wnd *child =
                windows::wnd_bindings.object_from_handle(control);
            if (dynamic_cast<native::check *>(child) ||
                dynamic_cast<native::radio *>(child)) {
                HDC hdc = reinterpret_cast<HDC>(wparam);
                SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
                SetBkMode(hdc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(
                    GetSysColorBrush(COLOR_WINDOW));
            }
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            wnd->destroy();
            return 0;

        case WM_DESTROY:
            wnd->on_native_destroy();
            wnd_bindings.unregister_by_handle(hwnd);
            if (wnd == native::app::main_wnd())
                PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
} // namespace windows

namespace native
{
    void wnd::apply_position() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            SetWindowPos(hwnd,
                         nullptr,
                         _bounds.p.x,
                         _bounds.p.y,
                         0,
                         0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    void wnd::apply_dimensions() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            LONG width = _bounds.d.w;
            LONG height = _bounds.d.h;
            if (dynamic_cast<app_wnd *>(this))
                windows::app_window_frame_size(hwnd, width, height);
            SetWindowPos(hwnd,
                         nullptr,
                         0,
                         0,
                         width,
                         height,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    void wnd::apply_bounds() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            LONG width = _bounds.d.w;
            LONG height = _bounds.d.h;
            if (dynamic_cast<app_wnd *>(this))
                windows::app_window_frame_size(hwnd, width, height);
            SetWindowPos(hwnd,
                         nullptr,
                         _bounds.p.x,
                         _bounds.p.y,
                         width,
                         height,
                         SWP_NOZORDER);
        }
    }

    void wnd::apply_parent() {
        HWND child = windows::wnd_bindings.handle_from_object(this);
        HWND parent =
            _parent ? windows::wnd_bindings.handle_from_object(_parent)
                    : nullptr;
        if (child)
            SetParent(child, parent);
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (hwnd)
            InvalidateRect(hwnd, nullptr, FALSE);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (hwnd) {
            RECT rect = {r.p.x, r.p.y, r.x2(), r.y2()};
            InvalidateRect(hwnd, &rect, FALSE);
        }
        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }
} // namespace native
