//
// Implements persistent Window Maker application menus. WINGs' popup
// selector is intentionally not used: it is a press-and-drag chooser,
// whereas an application menu opens on click and remains posted.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/app_wnd.h>
#include <native/menu.h>

#include "globals.h"

namespace
{
    linux::wmaker::native_menu *open_menu = nullptr;
    native::app_wnd *active_owner = nullptr;

    std::uint32_t next_id() {
        static std::uint32_t current = 0;
        return ++current;
    }

    W_Screen *wing_screen() {
        return reinterpret_cast<W_Screen *>(linux::wmaker::screen);
    }

    int width_of(WMFont *font, const std::string &text) {
        return text.empty()
                   ? 0
                   : WMWidthOfString(font,
                                     text.c_str(),
                                     static_cast<int>(text.size()));
    }

    char mnemonic_of(const std::string &text, std::size_t index) {
        if (index >= text.size())
            return 0;
        return static_cast<char>(std::tolower(
            static_cast<unsigned char>(text[index])));
    }

    void draw_text_with_mnemonic(Drawable target,
                                 WMFont *font,
                                 const std::string &text,
                                 std::size_t mnemonic,
                                 int x,
                                 int y) {
        W_Screen *native_screen = wing_screen();
        if (!native_screen || !font || target == None)
            return;
        WMDrawString(linux::wmaker::screen,
                     target,
                     native_screen->black,
                     font,
                     x,
                     y,
                     text.c_str(),
                     static_cast<int>(text.size()));
        if (mnemonic >= text.size())
            return;
        const std::string prefix = text.substr(0, mnemonic);
        const std::string letter = text.substr(mnemonic, 1);
        const int underline_x = x + width_of(font, prefix);
        const int underline_y = y +
                                static_cast<int>(WMFontHeight(font)) - 1;
        XDrawLine(linux::wmaker::display,
                  target,
                  WMColorGC(native_screen->black),
                  underline_x,
                  underline_y,
                  underline_x + std::max(1, width_of(font, letter)) - 1,
                  underline_y);
    }

    bool title_active(const linux::wmaker::menu_callback &callback) {
        return callback.menu && callback.menu->open_top ==
                                    static_cast<int>(callback.top_index);
    }

    void paint_title(linux::wmaker::menu_callback &callback) {
        if (!callback.title_widget || !callback.font ||
            !callback.menu || callback.top_index >=
                                  callback.menu->tops.size())
            return;
        const Window target = WMWidgetXID(callback.title_widget);
        if (target == None)
            return;
        W_Screen *native_screen = wing_screen();
        WMColor *background = callback.hot || title_active(callback)
                                  ? native_screen->white
                                  : native_screen->gray;
        XFillRectangle(linux::wmaker::display,
                       target,
                       WMColorGC(background),
                       0,
                       0,
                       WMWidgetWidth(callback.title_widget),
                       WMWidgetHeight(callback.title_widget));
        const auto &top = callback.menu->tops[callback.top_index];
        const int y = std::max(
            0,
            (static_cast<int>(WMWidgetHeight(callback.title_widget)) -
             static_cast<int>(WMFontHeight(callback.font))) /
                2);
        draw_text_with_mnemonic(target,
                                callback.font,
                                top.title,
                                top.mnemonic_index,
                                6,
                                y);
    }

    void repaint_titles(linux::wmaker::native_menu &menu) {
        for (auto *callback : menu.callbacks) {
            if (callback)
                paint_title(*callback);
        }
    }

    int popup_row_height(const native::main_menu::menu_entry &item,
                         int command_height) {
        return item.separator ? 9 : command_height;
    }

    int next_command(const std::vector<native::main_menu::menu_entry> &items,
                     int current,
                     int direction) {
        if (items.empty())
            return -1;
        int candidate = current;
        for (std::size_t count = 0; count < items.size(); ++count) {
            candidate = candidate < 0
                ? (direction > 0 ? 0 : static_cast<int>(items.size())-1)
                : (candidate + direction + static_cast<int>(items.size())) %
                      static_cast<int>(items.size());
            if (!items[static_cast<std::size_t>(candidate)].separator)
                return candidate;
        }
        return -1;
    }

    int item_at(const linux::wmaker::native_menu &menu,
                int x,
                int y) {
        if (menu.open_top < 0 ||
            menu.open_top >= static_cast<int>(menu.tops.size()) ||
            x < 2 || x >= menu.popup_width - 2 || y < 2 ||
            y >= menu.popup_height - 2 || menu.item_height <= 0)
            return -1;
        const auto &items = menu.tops[menu.open_top].items;
        int row_y = 2;
        for (std::size_t index = 0; index < items.size(); ++index) {
            const int height = popup_row_height(items[index],
                                                menu.item_height);
            if (y >= row_y && y < row_y+height)
                return items[index].separator
                    ? -1 : static_cast<int>(index);
            row_y += height;
        }
        return -1;
    }

    int top_at(const linux::wmaker::native_menu &menu,
               int root_x,
               int root_y) {
        for (std::size_t index = 0;
             index < menu.titles.size(); ++index) {
            WMFrame *title = menu.titles[index];
            if (!title || WMWidgetXID(title) == None)
                continue;
            int x = 0;
            int y = 0;
            Window child = None;
            XTranslateCoordinates(
                linux::wmaker::display,
                WMWidgetXID(title),
                RootWindow(linux::wmaker::display,
                           DefaultScreen(linux::wmaker::display)),
                0,
                0,
                &x,
                &y,
                &child);
            if (root_x >= x &&
                root_x < x + static_cast<int>(WMWidgetWidth(title)) &&
                root_y >= y &&
                root_y < y + static_cast<int>(WMWidgetHeight(title))) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    void paint_popup(linux::wmaker::native_menu &menu) {
        if (menu.popup == None || menu.open_top < 0 ||
            menu.open_top >= static_cast<int>(menu.tops.size()))
            return;
        W_Screen *native_screen = wing_screen();
        const auto &items = menu.tops[menu.open_top].items;
        XFillRectangle(linux::wmaker::display,
                       menu.popup,
                       WMColorGC(native_screen->gray),
                       0,
                       0,
                       menu.popup_width,
                       menu.popup_height);
        WMFont *font = WMDefaultSystemFont(linux::wmaker::screen);
        int y = 2;
        for (std::size_t index = 0; index < items.size(); ++index) {
            const int row_height = popup_row_height(items[index],
                                                    menu.item_height);
            if (items[index].separator) {
                const int line_y = y + row_height/2;
                XDrawLine(linux::wmaker::display, menu.popup,
                          WMColorGC(native_screen->darkGray),
                          6, line_y, menu.popup_width-7, line_y);
                XDrawLine(linux::wmaker::display, menu.popup,
                          WMColorGC(native_screen->white),
                          6, line_y+1, menu.popup_width-7, line_y+1);
                y += row_height;
                continue;
            }
            if (static_cast<int>(index) == menu.hot_item) {
                XFillRectangle(linux::wmaker::display,
                               menu.popup,
                               WMColorGC(native_screen->white),
                               2,
                               y,
                               menu.popup_width - 4,
                               row_height);
            }
            const int text_y = y + std::max(
                0,
                (row_height -
                 static_cast<int>(WMFontHeight(font))) /
                    2);
            draw_text_with_mnemonic(menu.popup,
                                    font,
                                    items[index].label,
                                    items[index].mnemonic_index,
                                    10,
                                    text_y);
            if (!items[index].shortcut.empty()) {
                WMDrawString(
                    linux::wmaker::screen,
                    menu.popup,
                    native_screen->black,
                    font,
                    menu.popup_width - 10 -
                        width_of(font, items[index].shortcut),
                    text_y,
                    items[index].shortcut.c_str(),
                    static_cast<int>(items[index].shortcut.size()));
            }
            y += row_height;
        }
        XDrawRectangle(linux::wmaker::display,
                       menu.popup,
                       WMColorGC(native_screen->black),
                       0,
                       0,
                       menu.popup_width - 1,
                       menu.popup_height - 1);
        if (menu.popup_width > 3 && menu.popup_height > 3) {
            XDrawLine(linux::wmaker::display,
                      menu.popup,
                      WMColorGC(native_screen->white),
                      1,
                      1,
                      menu.popup_width - 2,
                      1);
            XDrawLine(linux::wmaker::display,
                      menu.popup,
                      WMColorGC(native_screen->white),
                      1,
                      1,
                      1,
                      menu.popup_height - 2);
            XDrawLine(linux::wmaker::display,
                      menu.popup,
                      WMColorGC(native_screen->darkGray),
                      1,
                      menu.popup_height - 2,
                      menu.popup_width - 2,
                      menu.popup_height - 2);
            XDrawLine(linux::wmaker::display,
                      menu.popup,
                      WMColorGC(native_screen->darkGray),
                      menu.popup_width - 2,
                      1,
                      menu.popup_width - 2,
                      menu.popup_height - 2);
        }
    }

    void close_popup(linux::wmaker::native_menu &menu) {
        if (menu.popup != None)
            XUnmapWindow(linux::wmaker::display, menu.popup);
        XUngrabPointer(linux::wmaker::display, CurrentTime);
        XUngrabKeyboard(linux::wmaker::display, CurrentTime);
        menu.open_top = -1;
        menu.hot_item = -1;
        if (open_menu == &menu)
            open_menu = nullptr;
        repaint_titles(menu);
    }

    void invoke(linux::wmaker::native_menu &menu, int item_index) {
        if (menu.open_top < 0 ||
            menu.open_top >= static_cast<int>(menu.tops.size()))
            return;
        const auto &items = menu.tops[menu.open_top].items;
        if (item_index < 0 ||
            item_index >= static_cast<int>(items.size()) ||
            items[static_cast<std::size_t>(item_index)].separator)
            return;
        native::app_wnd *owner = menu.owner;
        const int command = items[static_cast<std::size_t>(item_index)].id;
        close_popup(menu);
        linux::wmaker::defer([owner, command]() {
            if (owner && owner->get_created())
                owner->on_native_menu(command);
        });
    }

    void open_popup(linux::wmaker::native_menu &menu,
                    std::size_t top_index) {
        if (top_index >= menu.tops.size() ||
            menu.tops[top_index].items.empty() ||
            !linux::wmaker::permit_input(menu.owner))
            return;
        if (open_menu && open_menu != &menu)
            close_popup(*open_menu);
        if (menu.open_top >= 0)
            close_popup(menu);

        WMFont *font = WMDefaultSystemFont(linux::wmaker::screen);
        menu.item_height = std::max(
            22, static_cast<int>(WMFontHeight(font)) + 8);
        int label_width = 0;
        int shortcut_width = 0;
        for (const auto &item : menu.tops[top_index].items) {
            label_width = std::max(label_width,
                                   width_of(font, item.label));
            shortcut_width = std::max(shortcut_width,
                                      width_of(font, item.shortcut));
        }
        menu.popup_width = std::max(
            180,
            20 + label_width +
                (shortcut_width > 0 ? 30 + shortcut_width : 0));
        menu.popup_height = 4;
        for (const auto &item : menu.tops[top_index].items)
            menu.popup_height += popup_row_height(item, menu.item_height);

        if (menu.popup == None) {
            XSetWindowAttributes attributes = {};
            attributes.override_redirect = True;
            attributes.save_under = True;
            attributes.background_pixel = WMColorPixel(
                wing_screen()->gray);
            attributes.border_pixel = WMColorPixel(
                wing_screen()->black);
            attributes.event_mask = ExposureMask | PointerMotionMask |
                                    ButtonPressMask | ButtonReleaseMask |
                                    KeyPressMask;
            menu.popup = XCreateWindow(
                linux::wmaker::display,
                RootWindow(linux::wmaker::display,
                           DefaultScreen(linux::wmaker::display)),
                0,
                0,
                1,
                1,
                0,
                CopyFromParent,
                InputOutput,
                CopyFromParent,
                CWOverrideRedirect | CWSaveUnder | CWBackPixel |
                    CWBorderPixel | CWEventMask,
                &attributes);
        }

        int title_x = 0;
        int title_y = 0;
        Window child = None;
        XTranslateCoordinates(
            linux::wmaker::display,
            WMWidgetXID(menu.titles[top_index]),
            RootWindow(linux::wmaker::display,
                       DefaultScreen(linux::wmaker::display)),
            0,
            0,
            &title_x,
            &title_y,
            &child);
        int x = title_x;
        int y = title_y + static_cast<int>(
                              WMWidgetHeight(menu.titles[top_index]));
        x = std::clamp(x,
                       0,
                       std::max(0,
                                static_cast<int>(WMScreenWidth(
                                    linux::wmaker::screen)) -
                                    menu.popup_width));
        if (y + menu.popup_height >
            static_cast<int>(WMScreenHeight(
                linux::wmaker::screen))) {
            y = std::max(0, title_y - menu.popup_height);
        }
        menu.popup_x = x;
        menu.popup_y = y;
        XMoveResizeWindow(linux::wmaker::display,
                          menu.popup,
                          x,
                          y,
                          menu.popup_width,
                          menu.popup_height);
        menu.open_top = static_cast<int>(top_index);
        menu.hot_item = -1;
        open_menu = &menu;
        XMapRaised(linux::wmaker::display, menu.popup);
        XGrabPointer(linux::wmaker::display,
                     menu.popup,
                     False,
                     PointerMotionMask | ButtonPressMask |
                         ButtonReleaseMask,
                     GrabModeAsync,
                     GrabModeAsync,
                     None,
                     None,
                     CurrentTime);
        XGrabKeyboard(linux::wmaker::display,
                      menu.popup,
                      False,
                      GrabModeAsync,
                      GrabModeAsync,
                      CurrentTime);
        repaint_titles(menu);
        paint_popup(menu);
    }

    void title_event(XEvent *event, void *client_data) {
        auto *callback =
            static_cast<linux::wmaker::menu_callback *>(client_data);
        if (!event || !callback || !callback->menu)
            return;
        if (event->type == EnterNotify)
            callback->hot = true;
        else if (event->type == LeaveNotify)
            callback->hot = false;
        else if (event->type == ButtonRelease &&
                 event->xbutton.button == Button1) {
            open_popup(*callback->menu, callback->top_index);
        }
        if ((event->type == Expose && event->xexpose.count == 0) ||
            event->type == EnterNotify || event->type == LeaveNotify ||
            event->type == ButtonRelease) {
            paint_title(*callback);
        }
    }

    void separator_event(XEvent *event, void *) {
        if (!event || event->type != Expose || event->xexpose.count != 0)
            return;
        W_Screen *native_screen = wing_screen();
        XFillRectangle(linux::wmaker::display,
                       event->xexpose.window,
                       WMColorGC(native_screen->darkGray),
                       event->xexpose.x,
                       event->xexpose.y,
                       static_cast<unsigned int>(event->xexpose.width),
                       static_cast<unsigned int>(event->xexpose.height));
    }

    bool shortcut_matches(const std::string &shortcut,
                          const XKeyEvent &event) {
        if (shortcut.empty())
            return false;
        const bool control = shortcut.find("Ctrl+") != std::string::npos;
        const bool alt = shortcut.find("Alt+") != std::string::npos;
        const bool shift = shortcut.find("Shift+") != std::string::npos;
        if (control != ((event.state & ControlMask) != 0) ||
            alt != ((event.state & Mod1Mask) != 0) ||
            shift != ((event.state & ShiftMask) != 0))
            return false;
        const std::size_t plus = shortcut.rfind('+');
        const std::string key = shortcut.substr(
            plus == std::string::npos ? 0 : plus + 1);
        XKeyEvent copy = event;
        const KeySym symbol = XLookupKeysym(&copy, 0);
        if (key.size() == 1) {
            const char *name = XKeysymToString(symbol);
            if (!name || !name[0])
                return false;
            return std::tolower(static_cast<unsigned char>(key[0])) ==
                   std::tolower(static_cast<unsigned char>(name[0]));
        }
        return key == "F4" && symbol == XK_F4;
    }

    linux::wmaker::native_menu *menu_for(native::app_wnd *owner) {
        return owner && owner->menu.id()
                   ? linux::wmaker::menu_bindings.object_from_handle(
                         owner->menu.id())
                   : nullptr;
    }

    bool handle_popup_key(linux::wmaker::native_menu &menu,
                          XKeyEvent &event) {
        XKeyEvent copy = event;
        const KeySym symbol = XLookupKeysym(&copy, 0);
        if (symbol == XK_Escape) {
            close_popup(menu);
        } else if (symbol == XK_Down) {
            menu.hot_item = next_command(
                menu.tops[menu.open_top].items, menu.hot_item, 1);
            paint_popup(menu);
        } else if (symbol == XK_Up) {
            menu.hot_item = next_command(
                menu.tops[menu.open_top].items, menu.hot_item, -1);
            paint_popup(menu);
        } else if (symbol == XK_Return || symbol == XK_KP_Enter) {
            if (menu.hot_item >= 0)
                invoke(menu, menu.hot_item);
        } else if (symbol == XK_Left || symbol == XK_Right) {
            const int count = static_cast<int>(menu.tops.size());
            const int next = (menu.open_top +
                              (symbol == XK_Left ? count - 1 : 1)) %
                             count;
            close_popup(menu);
            open_popup(menu, static_cast<std::size_t>(next));
        } else {
            const char *name = XKeysymToString(symbol);
            if (name && name[0]) {
                const char key = static_cast<char>(std::tolower(
                    static_cast<unsigned char>(name[0])));
                const auto &items = menu.tops[menu.open_top].items;
                for (std::size_t index = 0; index < items.size(); ++index) {
                    if (!items[index].separator &&
                        mnemonic_of(items[index].label,
                                    items[index].mnemonic_index) == key) {
                        invoke(menu, static_cast<int>(index));
                        break;
                    }
                }
            }
        }
        return true;
    }
} // namespace

namespace native
{
    main_menu::~main_menu() {
        detach();
    }

    void main_menu::detach() {
        auto *menu = _id
                         ? linux::wmaker::menu_bindings
                               .object_from_handle(_id)
                         : nullptr;
        if (menu) {
            if (open_menu == menu)
                close_popup(*menu);
            if (menu->popup != None)
                XDestroyWindow(linux::wmaker::display, menu->popup);
            for (WMFrame *title : menu->titles) {
                if (title)
                    WMDestroyWidget(title);
            }
            if (menu->separator)
                WMDestroyWidget(menu->separator);
            if (menu->background)
                WMDestroyWidget(menu->background);
            for (auto *callback : menu->callbacks)
                delete callback;
            delete menu;
        }
        if (_id)
            linux::wmaker::menu_bindings.unregister_by_handle(_id);
        _id = 0;
        _owner = nullptr;
    }

    void main_menu::attach(app_wnd &owner) {
        if (_id || _tops.empty())
            return;
        auto *window_state = linux::wmaker::state(&owner);
        if (!window_state || !window_state->window)
            return;

        auto *menu = new linux::wmaker::native_menu;
        menu->owner = &owner;
        menu->tops = _tops;
        int x = 0;
        WMFont *font = WMDefaultSystemFont(linux::wmaker::screen);
        menu->background = WMCreateFrame(window_state->window);
        WMSetFrameRelief(menu->background, WRFlat);
        WMSetWidgetBackgroundColor(menu->background,
                                   wing_screen()->gray);
        WMMoveWidget(menu->background, 0, 0);
        WMResizeWidget(
            menu->background,
            static_cast<unsigned int>(
                std::max<int>(1, owner.get_dimensions().w)),
            linux::wmaker::menu_bar_height);
        WMMapWidget(menu->background);

        for (std::size_t index = 0; index < _tops.size(); ++index) {
            const auto &top = _tops[index];
            const int width = std::max(
                32, width_of(font, top.title) + 12);
            WMFrame *title = WMCreateFrame(window_state->window);
            WMSetFrameRelief(title, WRFlat);
            WMSetWidgetBackgroundColor(title, wing_screen()->gray);
            WMMoveWidget(title, x, 0);
            WMResizeWidget(
                title, width, linux::wmaker::menu_bar_height);

            auto *callback = new linux::wmaker::menu_callback;
            callback->menu = menu;
            callback->title_widget = title;
            callback->top_index = index;
            callback->font = font;
            WMCreateEventHandler(
                WMWidgetView(title),
                ExposureMask | EnterWindowMask | LeaveWindowMask |
                    ButtonPressMask | ButtonReleaseMask,
                title_event,
                callback);
            WMMapWidget(title);
            menu->titles.push_back(title);
            menu->callbacks.push_back(callback);
            x += width;
        }

        // Task Manager and the other native GTK applications terminate the
        // otherwise-flat menu strip with one dark horizontal rule. Keep it a
        // separate sibling above the title widgets so their repaints cannot
        // erase it.
        menu->separator = WMCreateFrame(window_state->window);
        WMSetFrameRelief(menu->separator, WRFlat);
        WMSetWidgetBackgroundColor(menu->separator,
                                   wing_screen()->darkGray);
        WMMoveWidget(
            menu->separator,
            0,
            linux::wmaker::menu_bar_height - 1);
        WMResizeWidget(
            menu->separator,
            static_cast<unsigned int>(
                std::max<int>(1, owner.get_dimensions().w)),
            1);
        WMCreateEventHandler(
            WMWidgetView(menu->separator),
            ExposureMask,
            separator_event,
            nullptr);
        WMMapWidget(menu->separator);
        WMRaiseWidget(menu->separator);

        _owner = &owner;
        _id = next_id();
        window_state->menu_height = linux::wmaker::menu_bar_height;
        linux::wmaker::menu_bindings.register_pair(_id, menu);
    }
} // namespace native

namespace linux::wmaker
{
    bool handle_menu_event(XEvent &event) {
        if (open_menu) {
            if (event.type == Expose && event.xexpose.window ==
                                            open_menu->popup) {
                if (event.xexpose.count == 0)
                    paint_popup(*open_menu);
                return true;
            }
            if (event.type == MotionNotify) {
                const int top = top_at(*open_menu,
                                       event.xmotion.x_root,
                                       event.xmotion.y_root);
                if (top >= 0 && top != open_menu->open_top) {
                    native_menu *menu = open_menu;
                    open_popup(*menu, static_cast<std::size_t>(top));
                    return true;
                }
                const int next = item_at(*open_menu,
                                         event.xmotion.x_root -
                                             open_menu->popup_x,
                                         event.xmotion.y_root -
                                             open_menu->popup_y);
                if (next != open_menu->hot_item) {
                    open_menu->hot_item = next;
                    paint_popup(*open_menu);
                }
                return true;
            }
            if (event.type == ButtonPress)
                return true;
            if (event.type == ButtonRelease) {
                const int top = top_at(*open_menu,
                                       event.xbutton.x_root,
                                       event.xbutton.y_root);
                if (top >= 0 && top != open_menu->open_top) {
                    native_menu *menu = open_menu;
                    open_popup(*menu, static_cast<std::size_t>(top));
                    return true;
                }
                const int item = item_at(*open_menu,
                                         event.xbutton.x_root -
                                             open_menu->popup_x,
                                         event.xbutton.y_root -
                                             open_menu->popup_y);
                if (item >= 0)
                    invoke(*open_menu, item);
                else
                    close_popup(*open_menu);
                return true;
            }
            if (event.type == KeyPress)
                return handle_popup_key(*open_menu, event.xkey);
        }

        if (event.type != KeyPress)
            return false;
        native_menu *menu = menu_for(active_owner);
        if (!menu)
            return false;
        XKeyEvent key_event = event.xkey;
        const KeySym symbol = XLookupKeysym(&key_event, 0);
        const char *name = XKeysymToString(symbol);
        if ((event.xkey.state & Mod1Mask) != 0 && name && name[0]) {
            const char key = static_cast<char>(std::tolower(
                static_cast<unsigned char>(name[0])));
            for (std::size_t index = 0; index < menu->tops.size(); ++index) {
                if (mnemonic_of(menu->tops[index].title,
                                menu->tops[index].mnemonic_index) == key) {
                    open_popup(*menu, index);
                    return true;
                }
            }
        }
        for (std::size_t top = 0; top < menu->tops.size(); ++top) {
            for (std::size_t item = 0;
                 item < menu->tops[top].items.size(); ++item) {
                if (shortcut_matches(menu->tops[top].items[item].shortcut,
                                     event.xkey)) {
                    menu->open_top = static_cast<int>(top);
                    invoke(*menu, static_cast<int>(item));
                    return true;
                }
            }
        }
        return false;
    }

    void activate_menu_owner(native::app_wnd *owner) {
        active_owner = owner;
    }

    void resize_menu_bar(native::app_wnd *owner, int width) {
        if (!owner || !owner->menu.id())
            return;
        native_menu *menu = menu_bindings.object_from_handle(
            owner->menu.id());
        if (!menu || !menu->background)
            return;
        WMResizeWidget(menu->background,
                       static_cast<unsigned int>(std::max(1, width)),
                       static_cast<unsigned int>(
                           std::max(1, state(owner)
                                           ? state(owner)->menu_height
                                           : 24)));
        if (menu->separator) {
            WMResizeWidget(menu->separator,
                           static_cast<unsigned int>(std::max(1, width)),
                           1);
            WMRaiseWidget(menu->separator);
        }
    }
} // namespace linux::wmaker
