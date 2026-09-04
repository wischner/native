//
// Implements a focusable Athena host for custom collection controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "collection_host.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <native.h>

#include "globals.h"

namespace
{
    void resize_backbuffer(native::wnd &owner,
                           Widget widget,
                           int width,
                           int height) {
        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(
            &owner);
        if (!cache || width <= 0 || height <= 0 ||
            !linux::x11::cached_display || !XtIsRealized(widget)) {
            return;
        }
        if (cache->backbuffer && cache->buf_w == width &&
            cache->buf_h == height) {
            return;
        }
        if (cache->backbuffer)
            XFreePixmap(linux::x11::cached_display, cache->backbuffer);
        cache->backbuffer = XCreatePixmap(
            linux::x11::cached_display,
            XtWindow(widget),
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            DefaultDepthOfScreen(XtScreen(widget)));
        cache->buf_w = width;
        cache->buf_h = height;
    }

    void paint(native::wnd &owner, Widget widget) {
        Dimension width = 0;
        Dimension height = 0;
        XtVaGetValues(widget,
                      XtNwidth,
                      &width,
                      XtNheight,
                      &height,
                      nullptr);
        auto &graphics = owner.get_gpx();
        resize_backbuffer(owner, widget, width, height);
        native::rect invalid(0, 0, width, height);
        graphics.set_clip(invalid);
        native::wnd_paint_event event(invalid, graphics);
        owner.on_native_paint(event);

        auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(
            &owner);
        if (!cache || !cache->gc || !cache->backbuffer)
            return;
        XSetClipMask(linux::x11::cached_display, cache->gc, None);
        XCopyArea(linux::x11::cached_display,
                  cache->backbuffer,
                  XtWindow(widget),
                  cache->gc,
                  0,
                  0,
                  static_cast<unsigned int>(cache->buf_w),
                  static_cast<unsigned int>(cache->buf_h),
                  0,
                  0);
    }

    void key(native::wnd &owner, XKeyEvent &event) {
        const KeySym symbol = XLookupKeysym(&event, 0);
        if (auto *control =
                dynamic_cast<native::code_edit *>(&owner)) {
            const bool extend = (event.state & ShiftMask) != 0;
            const bool command = (event.state & ControlMask) != 0;
            if (command) {
                if (symbol == XK_a || symbol == XK_A)
                    control->on_native_key(
                        native::code_edit_key::select_all);
                else if (symbol == XK_c || symbol == XK_C)
                    control->on_native_key(native::code_edit_key::copy);
                else if (symbol == XK_x || symbol == XK_X)
                    control->on_native_key(native::code_edit_key::cut);
                else if (symbol == XK_v || symbol == XK_V)
                    control->on_native_key(native::code_edit_key::paste);
                else if (symbol == XK_z || symbol == XK_Z)
                    control->on_native_key(
                        extend ? native::code_edit_key::redo
                               : native::code_edit_key::undo);
                return;
            }
            native::code_edit_key command_key;
            bool handled = true;
            switch (symbol) {
            case XK_Left:
                command_key = native::code_edit_key::left;
                break;
            case XK_Right:
                command_key = native::code_edit_key::right;
                break;
            case XK_Up:
                command_key = native::code_edit_key::up;
                break;
            case XK_Down:
                command_key = native::code_edit_key::down;
                break;
            case XK_Home:
                command_key = native::code_edit_key::home;
                break;
            case XK_End:
                command_key = native::code_edit_key::end;
                break;
            case XK_Page_Up:
                command_key = native::code_edit_key::page_up;
                break;
            case XK_Page_Down:
                command_key = native::code_edit_key::page_down;
                break;
            case XK_BackSpace:
                command_key = native::code_edit_key::backspace;
                break;
            case XK_Delete:
                command_key = native::code_edit_key::delete_forward;
                break;
            case XK_Return:
            case XK_KP_Enter:
                command_key = native::code_edit_key::enter;
                break;
            case XK_Tab:
                command_key = native::code_edit_key::tab;
                break;
            case XK_Escape:
                command_key = native::code_edit_key::escape;
                break;
            default:
                handled = false;
                break;
            }
            if (handled) {
                control->on_native_key(command_key, extend);
                return;
            }
            char text[64]{};
            KeySym translated = NoSymbol;
            const int count = XLookupString(
                &event, text, sizeof(text), &translated, nullptr);
            if (count > 0) {
                control->on_native_text_input(std::string(
                    text, static_cast<std::size_t>(count)));
            }
            return;
        }
        if (auto *control =
                dynamic_cast<native::table_view *>(&owner)) {
            const bool extend = (event.state & ShiftMask) != 0;
            if ((event.state & ControlMask) != 0 &&
                (symbol == XK_a || symbol == XK_A)) {
                control->on_native_navigation(
                    native::table_navigation::select_all);
                return;
            }
            switch (symbol) {
            case XK_Up:
                control->on_native_navigation(
                    native::table_navigation::up, extend);
                break;
            case XK_Down:
                control->on_native_navigation(
                    native::table_navigation::down, extend);
                break;
            case XK_Home:
                control->on_native_navigation(
                    native::table_navigation::home, extend);
                break;
            case XK_End:
                control->on_native_navigation(
                    native::table_navigation::end, extend);
                break;
            case XK_Page_Up:
                control->on_native_navigation(
                    native::table_navigation::page_up, extend);
                break;
            case XK_Page_Down:
                control->on_native_navigation(
                    native::table_navigation::page_down, extend);
                break;
            case XK_Left:
                control->on_native_navigation(
                    native::table_navigation::collapse);
                break;
            case XK_Right:
                control->on_native_navigation(
                    native::table_navigation::expand);
                break;
            case XK_space:
                control->on_native_navigation(
                    native::table_navigation::toggle);
                break;
            case XK_Return:
            case XK_KP_Enter:
                control->on_native_navigation(
                    native::table_navigation::activate);
                break;
            default: {
                char text[64]{};
                KeySym translated = NoSymbol;
                const int count = XLookupString(
                    &event, text, sizeof(text), &translated, nullptr);
                if (count > 0 &&
                    (event.state & ControlMask) == 0) {
                    control->on_native_type_text(
                        std::string(text,
                                    static_cast<std::size_t>(count)));
                }
                break;
            }
            }
            return;
        }
        if (auto *control = dynamic_cast<native::accordion *>(&owner)) {
            switch (symbol) {
            case XK_Up:
                control->on_native_navigation(
                    native::accordion_navigation::previous);
                break;
            case XK_Down:
                control->on_native_navigation(
                    native::accordion_navigation::next);
                break;
            case XK_Home:
                control->on_native_navigation(
                    native::accordion_navigation::first);
                break;
            case XK_End:
                control->on_native_navigation(
                    native::accordion_navigation::last);
                break;
            case XK_Return:
            case XK_KP_Enter:
            case XK_space:
                control->on_native_navigation(
                    native::accordion_navigation::toggle);
                break;
            }
            return;
        }
        if (auto *control = dynamic_cast<native::tree_view *>(&owner)) {
            switch (symbol) {
            case XK_Up:
                control->on_native_navigation(
                    native::tree_view_navigation::up);
                break;
            case XK_Down:
                control->on_native_navigation(
                    native::tree_view_navigation::down);
                break;
            case XK_Left:
                control->on_native_navigation(
                    native::tree_view_navigation::left);
                break;
            case XK_Right:
                control->on_native_navigation(
                    native::tree_view_navigation::right);
                break;
            case XK_Home:
                control->on_native_navigation(
                    native::tree_view_navigation::home);
                break;
            case XK_End:
                control->on_native_navigation(
                    native::tree_view_navigation::end);
                break;
            case XK_Page_Up:
                control->on_native_navigation(
                    native::tree_view_navigation::page_up);
                break;
            case XK_Page_Down:
                control->on_native_navigation(
                    native::tree_view_navigation::page_down);
                break;
            case XK_space:
                control->on_native_navigation(
                    native::tree_view_navigation::toggle);
                break;
            case XK_Return:
            case XK_KP_Enter:
                control->on_native_navigation(
                    native::tree_view_navigation::activate);
                break;
            }
            return;
        }
        auto *control = dynamic_cast<native::icon_view *>(&owner);
        if (!control)
            return;
        switch (symbol) {
        case XK_Left:
            control->on_native_navigation(
                native::icon_view_navigation::left);
            break;
        case XK_Right:
            control->on_native_navigation(
                native::icon_view_navigation::right);
            break;
        case XK_Up:
            control->on_native_navigation(
                native::icon_view_navigation::up);
            break;
        case XK_Down:
            control->on_native_navigation(
                native::icon_view_navigation::down);
            break;
        case XK_Home:
            control->on_native_navigation(
                native::icon_view_navigation::home);
            break;
        case XK_End:
            control->on_native_navigation(
                native::icon_view_navigation::end);
            break;
        case XK_Page_Up:
            control->on_native_navigation(
                native::icon_view_navigation::page_up);
            break;
        case XK_Page_Down:
            control->on_native_navigation(
                native::icon_view_navigation::page_down);
            break;
        case XK_Return:
        case XK_KP_Enter:
            control->on_native_activate(control->get_selected_index());
            break;
        }
    }

    void route_event(Widget widget,
                     XtPointer client_data,
                     XEvent *event,
                     Boolean *) {
        auto *owner = static_cast<native::wnd *>(client_data);
        if (!owner || !event)
            return;
        switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0)
                paint(*owner, widget);
            break;
        case ConfigureNotify:
            resize_backbuffer(*owner,
                              widget,
                              event->xconfigure.width,
                              event->xconfigure.height);
            owner->on_native_resize(native::size(
                static_cast<native::dim>(event->xconfigure.width),
                static_cast<native::dim>(event->xconfigure.height)));
            break;
        case FocusIn:
            owner->on_native_focus(true);
            break;
        case FocusOut:
            owner->on_native_focus(false);
            break;
        case KeyPress:
            key(*owner, event->xkey);
            break;
        case MotionNotify:
            owner->on_native_mouse_move(native::point(
                event->xmotion.x, event->xmotion.y));
            break;
        case ButtonPress:
        case ButtonRelease: {
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_native_mouse_wheel(native::mouse_wheel_event(
                    native::point(event->xbutton.x, event->xbutton.y),
                    static_cast<native::coord>(
                        event->xbutton.button == Button4 ? 24 : -24),
                    native::wheel_direction::vertical));
                break;
            }
            if (event->xbutton.button != Button1)
                break;
            if (event->type == ButtonPress) {
                XSetInputFocus(event->xbutton.display,
                               event->xbutton.window,
                               RevertToParent,
                               event->xbutton.time);
            }
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                event->type == ButtonPress
                    ? native::mouse_action::press
                    : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            if (event->type != ButtonRelease)
                break;
            if (auto *table =
                    dynamic_cast<native::table_view *>(owner)) {
                auto *binding = linux::x11::table_view_bindings
                                    .object_from_handle(table);
                const auto selected = table->get_selected_rows();
                const native::table_row_id row = selected.empty()
                    ? native::invalid_table_row_id
                    : selected.back();
                if (binding &&
                    row != native::invalid_table_row_id &&
                    binding->last_row == row &&
                    event->xbutton.time - binding->last_click <= 400) {
                    table->on_native_activate(row);
                    binding->last_click = 0;
                    binding->last_row = native::invalid_table_row_id;
                } else if (binding) {
                    binding->last_click = event->xbutton.time;
                    binding->last_row = row;
                }
                break;
            }
            if (auto *tree =
                    dynamic_cast<native::tree_view *>(owner)) {
                auto *binding = linux::x11::tree_view_bindings
                                    .object_from_handle(tree);
                const native::tree_view_hit hit = tree->hit_test(
                    native::point(event->xbutton.x,
                                  event->xbutton.y));
                const native::tree_item_id item = hit.id;
                if (binding &&
                    hit.part == native::tree_view_hit_part::row &&
                    item != native::invalid_tree_item_id &&
                    binding->last_tree_item == item &&
                    event->xbutton.time - binding->last_click <= 400) {
                    tree->on_native_double_click(item);
                    binding->last_click = 0;
                    binding->last_tree_item =
                        native::invalid_tree_item_id;
                } else if (binding) {
                    binding->last_click = event->xbutton.time;
                    binding->last_tree_item =
                        hit.part == native::tree_view_hit_part::row
                            ? item
                            : native::invalid_tree_item_id;
                }
                break;
            }
            auto *icons = dynamic_cast<native::icon_view *>(owner);
            auto *binding = icons
                                ? linux::x11::icon_view_bindings
                                      .object_from_handle(icons)
                                : nullptr;
            if (!icons || !binding)
                break;
            const int item = icons->item_at(native::point(
                event->xbutton.x, event->xbutton.y));
            if (item >= 0 && binding->last_item == item &&
                event->xbutton.time - binding->last_click <= 400) {
                icons->on_native_activate(item);
                binding->last_click = 0;
                binding->last_item = -1;
            } else {
                binding->last_click = event->xbutton.time;
                binding->last_item = item;
            }
            break;
        }
        }
    }
} // namespace

namespace linux::x11
{
    Widget create_collection_host(native::wnd &owner,
                                  const char *name) {
        native::wnd *parent = owner.get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: collection requires a created parent.");
        Widget parent_widget = wnd_bindings.handle_from_object(parent);
        if (!parent_widget)
            throw std::runtime_error(
                "X11/Athena: collection parent has no widget.");
        const native::rect bounds = owner.get_bounds();
        Widget widget = XtVaCreateWidget(
            name,
            formWidgetClass,
            parent_widget,
            XtNhorizDistance,
            bounds.p.x,
            XtNvertDistance,
            bounds.p.y,
            XtNwidth,
            linux::x11::widget_dimension(bounds.d.w),
            XtNheight,
            linux::x11::widget_dimension(bounds.d.h),
            XtNborderWidth,
            0,
            XtNdefaultDistance,
            0,
            XtNleft,
            XtChainLeft,
            XtNright,
            XtChainLeft,
            XtNtop,
            XtChainTop,
            XtNbottom,
            XtChainTop,
            XtNresizable,
            False,
            nullptr);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: failed to create collection host.");
        XtAddEventHandler(widget,
                          ExposureMask | StructureNotifyMask |
                              FocusChangeMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask,
                          False,
                          route_event,
                          &owner);
        wnd_bindings.register_pair(widget, &owner);
        return widget;
    }
} // namespace linux::x11
