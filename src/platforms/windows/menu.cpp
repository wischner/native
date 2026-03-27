#include <windows.h>
#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace native {

main_menu::~main_menu()
{
    if (!_id) return;
    auto *m = win::menu_bindings.from_a(_id);
    if (m)
    {
        if (m->hmenu)
            DestroyMenu(m->hmenu);
        delete m;
    }
    win::menu_bindings.unregister_by_a(_id);
    _id = 0;
}

void main_menu::attach(app_wnd &owner)
{
    if (_id || _tops.empty()) return;
    _owner = &owner;

    HWND hwnd = win::wnd_bindings.from_b(&owner);
    if (!hwnd) return;

    HMENU hmenu = CreateMenu();
    for (const auto &top : _tops)
    {
        HMENU sub = CreatePopupMenu();
        for (const auto &item : top.items)
            AppendMenuA(sub, MF_STRING, (UINT_PTR)item.id, item.label.c_str());
        AppendMenuA(hmenu, MF_POPUP, (UINT_PTR)sub, top.title.c_str());
    }
    SetMenu(hwnd, hmenu);
    DrawMenuBar(hwnd);

    auto *h = new win::winmenu();
    h->hmenu = hmenu;
    h->owner = &owner;
    _id = next_id();
    win::menu_bindings.register_pair(_id, h);
}

} // namespace native
