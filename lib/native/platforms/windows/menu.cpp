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
                    AppendMenuA(sub,
                                MF_STRING,
                                (UINT_PTR)item.id,
                                display.c_str());
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
            AppendMenuA(hmenu, MF_POPUP, (UINT_PTR)sub, title.c_str());
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
