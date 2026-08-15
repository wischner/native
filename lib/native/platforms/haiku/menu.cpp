//
// Implements the Haiku menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <Window.h>

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace native {

main_menu::~main_menu() {
    detach();
}

void main_menu::detach() {
    if (!_id) {
        _owner = nullptr;
        return;
    }

    auto *m = haiku::menu_bindings.object_from_handle(_id);
    if (m) {
        if (m->owner) {
            haiku::owner_menu_bindings.unregister_by_handle(m->owner);
            BWindow *window = haiku::wnd_bindings.handle_from_object(m->owner);
            if (window && m->bar && window->Lock()) {
                if (m->bar->Window() == window) {
                    m->bar->RemoveSelf();
                    delete m->bar;
                }
                window->Unlock();
            }
        }
        delete m;
    }
    haiku::menu_bindings.unregister_by_handle(_id);
    _id = 0;
    _owner = nullptr;
}

void main_menu::attach(app_wnd &owner) {
    if (_id || _tops.empty()) return;
    _owner = &owner;

    BWindow *win = haiku::wnd_bindings.handle_from_object(&owner);
    if (!win) return;

    if (!win->Lock()) return;

    BRect bounds = win->Bounds();
    BMenuBar *bar = new BMenuBar(
        BRect(0, 0, bounds.Width(), 0),
        "menu_bar",
        B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
        B_ITEMS_IN_ROW,
        false);

    auto *h = new haiku::haiku_menu();
    h->owner = &owner;
    h->bar = bar;

    for (const auto &top : _tops) {
        BMenu *sub = new BMenu(top.title.c_str());
        for (const auto &item : top.items) {
            BMessage *msg = new BMessage(static_cast<uint32>(item.id));
            sub->AddItem(new BMenuItem(item.label.c_str(), msg));
            h->item_ids.insert(item.id);
        }
        bar->AddItem(sub);
    }

    win->AddChild(bar);
    bar->SetTargetForItems(win);

    win->Unlock();

    _id = next_id();
    haiku::menu_bindings.register_pair(_id, h);
    haiku::owner_menu_bindings.register_pair(&owner, h);
}

} // namespace native
