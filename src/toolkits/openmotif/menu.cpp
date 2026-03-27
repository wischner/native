#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace {
    struct MenuCallbackData {
        native::app_wnd *owner;
        int item_id;
    };

    void menu_callback(Widget, XtPointer client_data, XtPointer)
    {
        auto *d = static_cast<MenuCallbackData *>(client_data);
        if (d && d->owner)
            d->owner->on_menu.emit(d->item_id);
    }
} // namespace

namespace native {

main_menu::~main_menu()
{
    if (!_id) return;
    auto *m = motif::menu_bindings.from_a(_id);
    if (m)
    {
        if (m->menu_bar)
            XtDestroyWidget(m->menu_bar);
        delete m;
    }
    motif::menu_bindings.unregister_by_a(_id);
    _id = 0;
}

void main_menu::attach(app_wnd &owner)
{
    if (_id || _tops.empty()) return;
    _owner = &owner;

    Widget main_win = motif::main_wnd_bindings.from_b(&owner);
    if (!main_win) return;

    Widget menu_bar = XmCreateMenuBar(main_win, const_cast<char *>("menu_bar"), nullptr, 0);

    for (const auto &top : _tops)
    {
        Widget pulldown = XmCreatePulldownMenu(
            menu_bar,
            const_cast<char *>(top.title.c_str()),
            nullptr, 0);

        XmString label = XmStringCreateLocalized(const_cast<char *>(top.title.c_str()));
        Arg args[2];
        XtSetArg(args[0], XmNlabelString, label);
        XtSetArg(args[1], XmNsubMenuId, pulldown);
        Widget cascade = XmCreateCascadeButton(
            menu_bar,
            const_cast<char *>(top.title.c_str()),
            args, 2);
        XmStringFree(label);
        XtManageChild(cascade);

        for (const auto &item : top.items)
        {
            auto *cbd = new MenuCallbackData{&owner, item.id};
            XmString item_label = XmStringCreateLocalized(const_cast<char *>(item.label.c_str()));
            Arg item_args[1];
            XtSetArg(item_args[0], XmNlabelString, item_label);
            Widget btn = XmCreatePushButton(
                pulldown,
                const_cast<char *>(item.label.c_str()),
                item_args, 1);
            XmStringFree(item_label);
            XtAddCallback(btn, XmNactivateCallback, menu_callback, cbd);
            XtManageChild(btn);
        }
    }

    XtManageChild(menu_bar);
    XtVaSetValues(main_win, XmNmenuBar, menu_bar, nullptr);

    auto *hm     = new motif::motifmenu();
    hm->menu_bar = menu_bar;
    hm->owner    = &owner;
    _id = next_id();
    motif::menu_bindings.register_pair(_id, hm);
}

} // namespace native
