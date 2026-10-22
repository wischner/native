//
// Uses the Win32 flat toolbar common control for four-edge tool strips.
// Native buttons, alpha image lists and separators retain the OS theme.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../toolbar_peer.h"
#include <algorithm>
#include <typeinfo>
#include <stdexcept>

namespace
{
    struct toolbar_state;
    LRESULT CALLBACK command(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    struct toolbar_state
    {
        native::toolbar *owner = nullptr;
        HWND parent = nullptr, control = nullptr;
        HIMAGELIST images = nullptr;
        std::vector<native::tool_item> items;
        native::size icon_size;
        ~toolbar_state() {
            if (IsWindow(parent)) RemoveWindowSubclass(parent, command, 1);
            if (IsWindow(control)) DestroyWindow(control);
            if (images) ImageList_Destroy(images);
        }
    };
    LRESULT CALLBACK command(HWND window, UINT message, WPARAM wparam,
                              LPARAM lparam, UINT_PTR id, DWORD_PTR data) {
        auto &peer = *reinterpret_cast<toolbar_state *>(data);
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, command, id);
        if (message == WM_COMMAND && HWND(lparam) == peer.control) {
            const auto index = std::size_t(LOWORD(wparam) - 1);
            if (index < peer.owner->get_items().size()) {
                const auto item = peer.owner->get_items()[index];
                if (!peer.owner->on_native_command(item.id))
                    SendMessageW(peer.control, TB_CHECKBUTTON, index + 1, MAKELONG(item.checked, 0));
            }
            return 0;
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }
    bool same_items(const toolbar_state &peer, const native::toolbar &owner) {
        const auto &items = owner.get_items();
        if (peer.items.size() != items.size() || peer.icon_size.w != owner.get_icon_size().w ||
            peer.icon_size.h != owner.get_icon_size().h) return false;
        for (std::size_t index = 0; index < items.size(); ++index)
            if (peer.items[index].id != items[index].id || peer.items[index].text != items[index].text ||
                peer.items[index].kind != items[index].kind || peer.items[index].icon != items[index].icon)
                return false;
        return true;
    }
    HBITMAP bitmap(const native::img &image, native::size size) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = size.w; info.bmiHeader.biHeight = -LONG(size.h);
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void *bits = nullptr;
        auto result = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!result || !bits) return result;
        auto *bytes = static_cast<unsigned char *>(bits);
        std::fill(bytes, bytes + std::size_t(size.w) * size.h * 4, 0);
        const double scale = std::min(double(size.w) / image.w(), double(size.h) / image.h());
        const int width = std::max(1, int(image.w() * scale)), height = std::max(1, int(image.h() * scale));
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto color = image.pixels()[(y * image.h() / height) * image.w() + x * image.w() / width];
                auto *pixel = bytes + ((y + (size.h - height) / 2) * size.w + x + (size.w - width) / 2) * 4;
                pixel[0] = unsigned(color.b) * color.a / 255;
                pixel[1] = unsigned(color.g) * color.a / 255;
                pixel[2] = unsigned(color.r) * color.a / 255; pixel[3] = color.a;
            }
        return result;
    }
}
namespace native::detail
{
    bool toolbar_peer_active(const toolbar &owner, const canvas &surface) {
        return typeid(owner) == typeid(toolbar) && peer_state<toolbar_state>(surface);
    }
    bool refresh_toolbar_peer(toolbar &owner, canvas &surface, const std::vector<rect> &bounds) {
        if (typeid(owner) != typeid(toolbar) || !surface.get_created()) return false;
        auto *peer = peer_state<toolbar_state>(surface);
        const bool vertical = owner.get_edge() == window_edge::left || owner.get_edge() == window_edge::right;
        if (!peer) {
            INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
            InitCommonControlsEx(&controls);
            peer = new toolbar_state; peer->owner = &owner;
            peer->parent = windows::wnd_bindings.handle_from_object(&surface);
            assign_peer_state(surface, peer);
            peer->control = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS |
                    CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER | (vertical ? CCS_VERT : 0),
                0, 0, 0, 0, peer->parent, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!peer->control) throw std::runtime_error("Cannot create native toolbar.");
            SendMessageW(peer->control, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
            SendMessageW(peer->control, WM_SETFONT, WPARAM(windows::control_font()), TRUE);
            SendMessageW(peer->control, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER);
            SetWindowSubclass(peer->parent, command, 1, DWORD_PTR(peer));
        }
        if (!same_items(*peer, owner)) {
            while (SendMessageW(peer->control, TB_BUTTONCOUNT, 0, 0))
                SendMessageW(peer->control, TB_DELETEBUTTON, 0, 0);
            SendMessageW(peer->control, TB_SETIMAGELIST, 0, 0);
            if (peer->images) ImageList_Destroy(peer->images);
            peer->items = owner.get_items(); peer->icon_size = owner.get_icon_size();
            peer->images = ImageList_Create(peer->icon_size.w, peer->icon_size.h, ILC_COLOR32, 1, 4);
            SendMessageW(peer->control, TB_SETIMAGELIST, 0, LPARAM(peer->images));
            for (std::size_t index = 0; index < peer->items.size(); ++index) {
                const auto &item = peer->items[index];
                TBBUTTON tool{}; tool.idCommand = index + 1;
                tool.iBitmap = I_IMAGENONE;
                if (item.icon) {
                    auto image = bitmap(*item.icon, peer->icon_size);
                    tool.iBitmap = ImageList_Add(peer->images, image, nullptr); DeleteObject(image);
                }
                const auto text = windows::utf8_to_wide(item.text);
                tool.iString = item.text.empty() ? -1 : INT_PTR(text.c_str());
                tool.fsState = TBSTATE_ENABLED | (vertical ? TBSTATE_WRAP : 0);
                tool.fsStyle = item.kind == tool_kind::separator ? BTNS_SEP
                    : BTNS_AUTOSIZE | (item.text.empty() ? 0 : BTNS_SHOWTEXT) |
                        (item.kind == tool_kind::button ? BTNS_BUTTON : BTNS_CHECK);
                SendMessageW(peer->control, TB_ADDBUTTONSW, 1, LPARAM(&tool));
            }
        }
        const auto size = surface.get_dimensions();
        SendMessageW(peer->control, TB_SETBUTTONSIZE, 0,
            MAKELONG(vertical ? std::max(1, int(size.w) - 4) : owner.get_icon_size().w + 8,
                     vertical ? owner.get_icon_size().h + 8 : std::max(1, int(size.h) - 4)));
        for (std::size_t index = 0; index < owner.get_items().size(); ++index) {
            const auto &item = owner.get_items()[index];
            SendMessageW(peer->control, TB_ENABLEBUTTON, index + 1, MAKELONG(item.enabled && surface.get_input_enabled(), 0));
            SendMessageW(peer->control, TB_CHECKBUTTON, index + 1, MAKELONG(item.checked, 0));
            if (index < bounds.size() && item.kind != tool_kind::separator) {
                TBBUTTONINFOW info{}; info.cbSize = sizeof(info); info.dwMask = TBIF_SIZE;
                info.cx = vertical ? std::max(1, int(size.w) - 4) : bounds[index].w();
                SendMessageW(peer->control, TB_SETBUTTONINFOW, index + 1, LPARAM(&info));
            }
        }
        MoveWindow(peer->control, 0, 0, size.w, size.h, TRUE);
        return true;
    }
}
