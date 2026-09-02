//
// Implements X11 application menus with Athena MenuButton, SimpleMenu,
// and SmeBSB widgets. Athena owns popup placement, input grabs,
// highlighting, painting, and menu dismissal.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#include <native.h>
#include <native/menu.h>

#include "globals.h"
#include "../../menu_shortcut.h"

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
            callback->owner->on_native_menu(callback->item_id);
    }

    bool shortcut_matches(const std::string &value,
                          XKeyEvent &event) {
        const auto shortcut = native::detail::parse_menu_shortcut(value);
        if (shortcut.key.empty() ||
            shortcut.control != ((event.state & ControlMask) != 0) ||
            shortcut.alt != ((event.state & Mod1Mask) != 0) ||
            shortcut.shift != ((event.state & ShiftMask) != 0))
            return false;
        const KeySym symbol = XLookupKeysym(&event, 0);
        if (shortcut.key.size() == 1) {
            const char *name = XKeysymToString(symbol);
            return name && name[0] &&
                std::tolower(static_cast<unsigned char>(name[0])) ==
                std::tolower(static_cast<unsigned char>(shortcut.key[0]));
        }
        return (shortcut.key == "F4" || shortcut.key == "f4") &&
            symbol == XK_F4;
    }

    void accelerator_event(Widget,
                           XtPointer client_data,
                           XEvent *event,
                           Boolean *) {
        auto *menu = static_cast<linux::x11::xaw_menu *>(client_data);
        if (!menu || !menu->owner || !event || event->type != KeyPress)
            return;
        for (const auto &top : menu->owner->menu.tops()) {
            for (const auto &item : top.items) {
                if (!item.separator &&
                    shortcut_matches(item.shortcut, event->xkey)) {
                    menu->owner->on_native_menu(item.id);
                    return;
                }
            }
        }
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
            if (menu->event_widget)
                XtRemoveEventHandler(menu->event_widget,
                                     KeyPressMask,
                                     False,
                                     accelerator_event,
                                     menu);
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
        native_menu->event_widget = main_window;
        native_menu->owner = &owner;
        XtAddEventHandler(main_window,
                          KeyPressMask,
                          False,
                          accelerator_event,
                          native_menu);

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
                if (item.separator) {
                    XtVaCreateManagedWidget("menu_separator",
                                            smeLineObjectClass,
                                            popup,
                                            nullptr);
                    continue;
                }
                auto *callback =
                    new linux::x11::xaw_menu_callback{&owner, item.id};
                native_menu->callbacks.push_back(callback);

                const std::string display = item.shortcut.empty()
                    ? item.label : item.label + "    " + item.shortcut;
                Widget entry =
                    XtVaCreateManagedWidget("menu_item",
                                            smeBSBObjectClass,
                                            popup,
                                            XtNlabel,
                                            display.c_str(),
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
