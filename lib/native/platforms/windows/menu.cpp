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
        for (const auto &top : _tops) {
            HMENU sub = CreatePopupMenu();
            for (const auto &item : top.items)
                AppendMenuA(sub,
                            MF_STRING,
                            (UINT_PTR)item.id,
                            item.label.c_str());
            AppendMenuA(
                hmenu, MF_POPUP, (UINT_PTR)sub, top.title.c_str());
        }
        SetMenu(hwnd, hmenu);
        DrawMenuBar(hwnd);

        auto *h = new windows::win_menu();
        h->hmenu = hmenu;
        h->owner = &owner;
        _id = next_id();
        windows::menu_bindings.register_pair(_id, h);
    }

} // namespace native
