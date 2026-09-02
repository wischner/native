//
// Implements the OpenMotif menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

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

namespace
{
    void menu_callback(Widget, XtPointer client_data, XtPointer) {
        auto *d = static_cast<linux::openmotif::motif_menu_callback *>(
            client_data);
        if (d && d->owner)
            d->owner->on_native_menu(d->item_id);
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

        auto *m =
            linux::openmotif::menu_bindings.object_from_handle(_id);
        if (m) {
            if (m->menu_bar)
                XtDestroyWidget(m->menu_bar);
            for (auto *callback : m->callbacks)
                delete callback;
            delete m;
        }
        linux::openmotif::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;
        _owner = &owner;

        Widget main_win =
            linux::openmotif::main_wnd_bindings.handle_from_object(
                &owner);
        if (!main_win)
            return;

        Widget menu_bar = XmCreateMenuBar(
            main_win, const_cast<char *>("menu_bar"), nullptr, 0);

        auto *hm = new linux::openmotif::motif_menu();
        hm->menu_bar = menu_bar;
        hm->owner = &owner;

        for (const auto &top : _tops) {
            Widget pulldown = XmCreatePulldownMenu(
                menu_bar,
                const_cast<char *>(top.title.c_str()),
                nullptr,
                0);

            XmString label = XmStringCreateLocalized(
                const_cast<char *>(top.title.c_str()));
            Arg args[3];
            int arg_count = 0;
            XtSetArg(args[arg_count], XmNlabelString, label);
            ++arg_count;
            XtSetArg(args[arg_count], XmNsubMenuId, pulldown);
            ++arg_count;
            if (top.mnemonic_index < top.title.size()) {
                XtSetArg(args[arg_count], XmNmnemonic,
                         top.title[top.mnemonic_index]);
                ++arg_count;
            }
            Widget cascade = XmCreateCascadeButton(
                menu_bar,
                const_cast<char *>(top.title.c_str()),
                args,
                arg_count);
            XmStringFree(label);
            XtManageChild(cascade);

            for (const auto &item : top.items) {
                if (item.separator) {
                    Widget separator = XmCreateSeparator(
                        pulldown, const_cast<char *>("separator"),
                        nullptr, 0);
                    XtManageChild(separator);
                    continue;
                }
                auto *cbd = new linux::openmotif::motif_menu_callback{
                    &owner, item.id};
                hm->callbacks.push_back(cbd);
                XmString item_label = XmStringCreateLocalized(
                    const_cast<char *>(item.label.c_str()));
                Arg item_args[5];
                int item_arg_count = 0;
                XtSetArg(item_args[item_arg_count], XmNlabelString,
                         item_label);
                ++item_arg_count;
                if (item.mnemonic_index < item.label.size()) {
                    XtSetArg(item_args[item_arg_count], XmNmnemonic,
                             item.label[item.mnemonic_index]);
                    ++item_arg_count;
                }
                XmString accelerator_text = nullptr;
                std::string accelerator;
                if (!item.shortcut.empty()) {
                    const auto parsed = native::detail::parse_menu_shortcut(
                        item.shortcut);
                    if (parsed.control) accelerator += "Ctrl ";
                    if (parsed.alt) accelerator += "Alt ";
                    if (parsed.shift) accelerator += "Shift ";
                    accelerator += "<Key>" + parsed.key;
                    accelerator_text = XmStringCreateLocalized(
                        const_cast<char *>(item.shortcut.c_str()));
                    XtSetArg(item_args[item_arg_count], XmNaccelerator,
                             accelerator.c_str());
                    ++item_arg_count;
                    XtSetArg(item_args[item_arg_count], XmNacceleratorText,
                             accelerator_text);
                    ++item_arg_count;
                }
                Widget btn = XmCreatePushButton(
                    pulldown,
                    const_cast<char *>(item.label.c_str()),
                    item_args,
                    item_arg_count);
                XmStringFree(item_label);
                if (accelerator_text)
                    XmStringFree(accelerator_text);
                XtAddCallback(
                    btn, XmNactivateCallback, menu_callback, cbd);
                XtManageChild(btn);
            }
        }

        XtManageChild(menu_bar);
        XtVaSetValues(main_win, XmNmenuBar, menu_bar, nullptr);

        _id = next_id();
        linux::openmotif::menu_bindings.register_pair(_id, hm);
    }

} // namespace native
