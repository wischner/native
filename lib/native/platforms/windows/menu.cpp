//
// Implements the Windows menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <windows.h>
#include <native.h>
#include <native/menu.h>
#include "globals.h"
#include "../../menu_shortcut.h"

namespace
{
    uint32_t next_id() {
        static uint32_t c = 0;
        return ++c;
    }
} // namespace

namespace native
{

    void main_menu::apply_checked(int command) {
        auto *menu = windows::menu_bindings.object_from_handle(_id);
        if (menu && menu->hmenu)
            CheckMenuItem(menu->hmenu, static_cast<UINT>(command),
                          MF_BYCOMMAND | (get_checked(command)
                              ? MF_CHECKED : MF_UNCHECKED));
    }

    main_menu::~main_menu() {
        detach();
    }

    void main_menu::detach() {
        if (!_id) {
            _owner = nullptr;
            return;
        }

        auto *m = windows::menu_bindings.object_from_handle(_id);
        if (m) {
            if (m->accelerators)
                DestroyAcceleratorTable(m->accelerators);
            if (m->hmenu) {
                HWND owner =
                    windows::wnd_bindings.handle_from_object(m->owner);
                if (owner && GetMenu(owner) == m->hmenu) {
                    SetMenu(owner, nullptr);
                    DrawMenuBar(owner);
                }
                DestroyMenu(m->hmenu);
            }
            delete m;
        }
        windows::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;
        _owner = &owner;

        HWND hwnd = windows::wnd_bindings.handle_from_object(&owner);
        if (!hwnd)
            return;

        HMENU hmenu = CreateMenu();
        std::vector<ACCEL> accelerators;
        for (const auto &top : _tops) {
            HMENU sub = CreatePopupMenu();
            for (const auto &item : top.items) {
                if (item.separator)
                    AppendMenuA(sub, MF_SEPARATOR, 0, nullptr);
                else {
                    std::string display =
                        native::detail::decorate_menu_mnemonic(
                            item.label, item.mnemonic_index);
                    if (!item.shortcut.empty())
                        display += "\t" + item.shortcut;
                    AppendMenuW(sub,
                                MF_STRING | (item.checked
                                    ? MF_CHECKED : MF_UNCHECKED),
                                (UINT_PTR)item.id,
                                windows::utf8_to_wide(display).c_str());
                    const auto parsed = native::detail::parse_menu_shortcut(
                        item.shortcut);
                    WORD key = 0;
                    if (parsed.key.size() == 1)
                        key = static_cast<WORD>(std::toupper(
                            static_cast<unsigned char>(parsed.key[0])));
                    else if (parsed.key.size() > 1 &&
                             (parsed.key[0] == 'F' || parsed.key[0] == 'f'))
                        key = static_cast<WORD>(VK_F1 +
                            std::max(0, std::stoi(parsed.key.substr(1))-1));
                    if (key) {
                        BYTE flags = FVIRTKEY;
                        if (parsed.control) flags |= FCONTROL;
                        if (parsed.alt) flags |= FALT;
                        if (parsed.shift) flags |= FSHIFT;
                        accelerators.push_back(
                            {flags, key, static_cast<WORD>(item.id)});
                    }
                }
            }
            const std::string title =
                native::detail::decorate_menu_mnemonic(
                    top.title, top.mnemonic_index);
            AppendMenuW(hmenu, MF_POPUP, (UINT_PTR)sub,
                        windows::utf8_to_wide(title).c_str());
        }
        SetMenu(hwnd, hmenu);
        DrawMenuBar(hwnd);

        auto *h = new windows::win_menu();
        h->hmenu = hmenu;
        if (!accelerators.empty())
            h->accelerators = CreateAcceleratorTableW(
                accelerators.data(),
                static_cast<int>(accelerators.size()));
        h->owner = &owner;
        _id = next_id();
        windows::menu_bindings.register_pair(_id, h);
    }

} // namespace native
