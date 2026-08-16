//
// Implements X11 application menus with Athena MenuButton, SimpleMenu,
// and SmeBSB widgets. Athena owns popup placement, input grabs,
// highlighting, painting, and menu dismissal.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>

#include <native.h>
#include <native/menu.h>

#include "globals.h"

namespace
{
    uint32_t next_id() {
        static uint32_t current_id = 0;
        return ++current_id;
    }

    void menu_activate(Widget, XtPointer client_data, XtPointer) {
        auto *callback =
            static_cast<linux::x11::xaw_menu_callback *>(client_data);
        if (callback && callback->owner)
            callback->owner->on_menu.emit(callback->item_id);
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

        auto *menu = linux::x11::menu_bindings.object_from_handle(_id);
        if (menu) {
            Widget canvas =
                _owner ? linux::x11::wnd_bindings.handle_from_object(
                             _owner)
                       : nullptr;
            if (canvas) {
                XtVaSetValues(canvas,
                              XtNfromVert,
                              nullptr,
                              XtNvertDistance,
                              0,
                              nullptr);
            }
            if (menu->menu_bar)
                XtDestroyWidget(menu->menu_bar);
            for (auto *callback : menu->callbacks)
                delete callback;
            delete menu;
        }

        linux::x11::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;

        Widget main_window =
            linux::x11::main_wnd_bindings.handle_from_object(&owner);
        if (!main_window)
            return;

        Dimension menu_width = 0;
        XtVaGetValues(main_window, XtNwidth, &menu_width, nullptr);

        Widget menu_bar = XtVaCreateManagedWidget("menu_bar",
                                                  boxWidgetClass,
                                                  main_window,
                                                  XtNorientation,
                                                  XtorientHorizontal,
                                                  XtNhSpace,
                                                  0,
                                                  XtNvSpace,
                                                  0,
                                                  XtNhorizDistance,
                                                  0,
                                                  XtNvertDistance,
                                                  0,
                                                  XtNwidth,
                                                  menu_width,
                                                  XtNborderWidth,
                                                  0,
                                                  XtNleft,
                                                  XtChainLeft,
                                                  XtNright,
                                                  XtChainRight,
                                                  XtNtop,
                                                  XtChainTop,
                                                  nullptr);
        if (!menu_bar)
            return;

        auto *native_menu = new linux::x11::xaw_menu();
        native_menu->menu_bar = menu_bar;
        native_menu->owner = &owner;

        for (const auto &top : _tops) {
            Widget menu_button =
                XtVaCreateManagedWidget("menu_button",
                                        menuButtonWidgetClass,
                                        menu_bar,
                                        XtNlabel,
                                        top.title.c_str(),
                                        XtNmenuName,
                                        "menu",
                                        nullptr);

            Widget popup = XtVaCreatePopupShell(
                "menu", simpleMenuWidgetClass, menu_button, nullptr);

            for (const auto &item : top.items) {
                auto *callback =
                    new linux::x11::xaw_menu_callback{&owner, item.id};
                native_menu->callbacks.push_back(callback);

                Widget entry =
                    XtVaCreateManagedWidget("menu_item",
                                            smeBSBObjectClass,
                                            popup,
                                            XtNlabel,
                                            item.label.c_str(),
                                            nullptr);
                XtAddCallback(
                    entry, XtNcallback, menu_activate, callback);
            }
        }

        _owner = &owner;
        _id = next_id();
        linux::x11::menu_bindings.register_pair(_id, native_menu);

        // A menu attached after window creation must move the existing
        // Athena drawing form below the new menu bar too.
        Widget canvas =
            linux::x11::wnd_bindings.handle_from_object(&owner);
        if (canvas) {
            XtVaSetValues(canvas,
                          XtNfromVert,
                          menu_bar,
                          XtNvertDistance,
                          0,
                          nullptr);
        }
    }
} // namespace native
