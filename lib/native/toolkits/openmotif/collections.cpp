//
// Implements Motif collection and source-editor host event routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <string>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <Xm/DrawingA.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    linux::openmotif::motif_collection *binding(native::wnd &owner) {
        if (auto *accordion = dynamic_cast<native::accordion *>(&owner))
            return linux::openmotif::accordion_bindings
                .object_from_handle(accordion);
        if (auto *icons = dynamic_cast<native::icon_view *>(&owner))
            return linux::openmotif::icon_view_bindings
                .object_from_handle(icons);
        if (auto *tree = dynamic_cast<native::tree_view *>(&owner))
            return linux::openmotif::tree_view_bindings
                .object_from_handle(tree);
        if (auto *table = dynamic_cast<native::table_view *>(&owner))
            return linux::openmotif::table_view_bindings
                .object_from_handle(table);
        if (auto *editor = dynamic_cast<native::code_edit *>(&owner))
            return linux::openmotif::code_edit_bindings
                .object_from_handle(editor);
        return nullptr;
    }

    void resize_backbuffer(native::wnd &owner,
                           Widget widget,
                           int width,
                           int height) {
        auto *cache = linux::openmotif::wnd_gpx_bindings
                          .object_from_handle(&owner);
        if (!cache || !XtIsRealized(widget) || width <= 0 ||
            height <= 0)
            return;
        if (cache->backbuffer && cache->buf_w == width &&
            cache->buf_h == height)
            return;
        if (cache->backbuffer)
            XFreePixmap(linux::openmotif::cached_display,
                        cache->backbuffer);
        cache->backbuffer = XCreatePixmap(
            linux::openmotif::cached_display,
            XtWindow(widget),
            width,
            height,
            DefaultDepthOfScreen(XtScreen(widget)));
        cache->buf_w = width;
        cache->buf_h = height;
    }

    void paint(native::wnd &owner, Widget widget) {
        Dimension width = 0;
        Dimension height = 0;
        XtVaGetValues(widget,
                      XmNwidth,
                      &width,
                      XmNheight,
                      &height,
                      nullptr);
        auto &graphics = owner.get_gpx();
        resize_backbuffer(owner, widget, width, height);
        native::rect invalid(0, 0, width, height);
        graphics.set_clip(invalid);
        native::wnd_paint_event event(invalid, graphics);
        owner.on_wnd_paint.emit(event);
        auto *cache = linux::openmotif::wnd_gpx_bindings
                          .object_from_handle(&owner);
        if (!cache || !cache->gc || !cache->backbuffer)
            return;
        XSetClipMask(linux::openmotif::cached_display, cache->gc, None);
        XCopyArea(linux::openmotif::cached_display,
                  cache->backbuffer,
                  XtWindow(widget),
                  cache->gc,
                  0,
                  0,
                  cache->buf_w,
                  cache->buf_h,
                  0,
                  0);
    }

    void navigate(native::wnd &owner, KeySym symbol) {
        if (auto *editor =
                dynamic_cast<native::code_edit *>(&owner)) {
            native::code_edit_key key;
            bool handled = true;
            if (symbol == XK_Left)
                key = native::code_edit_key::left;
            else if (symbol == XK_Right)
                key = native::code_edit_key::right;
            else if (symbol == XK_Up)
                key = native::code_edit_key::up;
            else if (symbol == XK_Down)
                key = native::code_edit_key::down;
            else if (symbol == XK_Home)
                key = native::code_edit_key::home;
            else if (symbol == XK_End)
                key = native::code_edit_key::end;
            else if (symbol == XK_Page_Up)
                key = native::code_edit_key::page_up;
            else if (symbol == XK_Page_Down)
                key = native::code_edit_key::page_down;
            else if (symbol == XK_BackSpace)
                key = native::code_edit_key::backspace;
            else if (symbol == XK_Delete)
                key = native::code_edit_key::delete_forward;
            else if (symbol == XK_Return || symbol == XK_KP_Enter)
                key = native::code_edit_key::enter;
            else if (symbol == XK_Tab)
                key = native::code_edit_key::tab;
            else if (symbol == XK_Escape)
                key = native::code_edit_key::escape;
            else
                handled = false;
            if (handled)
                editor->on_native_key(key);
            return;
        }
        if (auto *table =
                dynamic_cast<native::table_view *>(&owner)) {
            if (symbol == XK_Up)
                table->on_native_navigation(
                    native::table_navigation::up);
            else if (symbol == XK_Down)
                table->on_native_navigation(
                    native::table_navigation::down);
            else if (symbol == XK_Home)
                table->on_native_navigation(
                    native::table_navigation::home);
            else if (symbol == XK_End)
                table->on_native_navigation(
                    native::table_navigation::end);
            else if (symbol == XK_Page_Up)
                table->on_native_navigation(
                    native::table_navigation::page_up);
            else if (symbol == XK_Page_Down)
                table->on_native_navigation(
                    native::table_navigation::page_down);
            else if (symbol == XK_Left)
                table->on_native_navigation(
                    native::table_navigation::collapse);
            else if (symbol == XK_Right)
                table->on_native_navigation(
                    native::table_navigation::expand);
            else if (symbol == XK_space)
                table->on_native_navigation(
                    native::table_navigation::toggle);
            else if (symbol == XK_Return || symbol == XK_KP_Enter)
                table->on_native_navigation(
                    native::table_navigation::activate);
            return;
        }
        if (auto *accordion =
                dynamic_cast<native::accordion *>(&owner)) {
            if (symbol == XK_Up)
                accordion->on_native_navigation(
                    native::accordion_navigation::previous);
            else if (symbol == XK_Down)
                accordion->on_native_navigation(
                    native::accordion_navigation::next);
            else if (symbol == XK_Home)
                accordion->on_native_navigation(
                    native::accordion_navigation::first);
            else if (symbol == XK_End)
                accordion->on_native_navigation(
                    native::accordion_navigation::last);
            else if (symbol == XK_Return || symbol == XK_KP_Enter ||
                     symbol == XK_space)
                accordion->on_native_navigation(
                    native::accordion_navigation::toggle);
            return;
        }
        if (auto *tree = dynamic_cast<native::tree_view *>(&owner)) {
            if (symbol == XK_Up)
                tree->on_native_navigation(
                    native::tree_view_navigation::up);
            else if (symbol == XK_Down)
                tree->on_native_navigation(
                    native::tree_view_navigation::down);
            else if (symbol == XK_Left)
                tree->on_native_navigation(
                    native::tree_view_navigation::left);
            else if (symbol == XK_Right)
                tree->on_native_navigation(
                    native::tree_view_navigation::right);
            else if (symbol == XK_Home)
                tree->on_native_navigation(
                    native::tree_view_navigation::home);
            else if (symbol == XK_End)
                tree->on_native_navigation(
                    native::tree_view_navigation::end);
            else if (symbol == XK_Page_Up)
                tree->on_native_navigation(
                    native::tree_view_navigation::page_up);
            else if (symbol == XK_Page_Down)
                tree->on_native_navigation(
                    native::tree_view_navigation::page_down);
            else if (symbol == XK_space)
                tree->on_native_navigation(
                    native::tree_view_navigation::toggle);
            else if (symbol == XK_Return || symbol == XK_KP_Enter)
                tree->on_native_navigation(
                    native::tree_view_navigation::activate);
            return;
        }
        auto *icons = dynamic_cast<native::icon_view *>(&owner);
        if (!icons)
            return;
        if (symbol == XK_Left)
            icons->on_native_navigation(native::icon_view_navigation::left);
        else if (symbol == XK_Right)
            icons->on_native_navigation(native::icon_view_navigation::right);
        else if (symbol == XK_Up)
            icons->on_native_navigation(native::icon_view_navigation::up);
        else if (symbol == XK_Down)
            icons->on_native_navigation(native::icon_view_navigation::down);
        else if (symbol == XK_Home)
            icons->on_native_navigation(native::icon_view_navigation::home);
        else if (symbol == XK_End)
            icons->on_native_navigation(native::icon_view_navigation::end);
        else if (symbol == XK_Page_Up)
            icons->on_native_navigation(
                native::icon_view_navigation::page_up);
        else if (symbol == XK_Page_Down)
            icons->on_native_navigation(
                native::icon_view_navigation::page_down);
        else if (symbol == XK_Return || symbol == XK_KP_Enter)
            icons->on_native_activate(icons->get_selected_index());
    }

    void handle_event(Widget widget,
                      XtPointer data,
                      XEvent *event,
                      Boolean *) {
        auto *owner = static_cast<native::wnd *>(data);
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
                event->xconfigure.width, event->xconfigure.height));
            break;
        case FocusIn:
        case FocusOut: {
            const bool focused = event->type == FocusIn;
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(owner))
                accordion->on_native_focus(focused);
            if (auto *icons = dynamic_cast<native::icon_view *>(owner))
                icons->on_native_focus(focused);
            if (auto *tree = dynamic_cast<native::tree_view *>(owner))
                tree->on_native_focus(focused);
            if (auto *table = dynamic_cast<native::table_view *>(owner))
                table->on_native_focus(focused);
            if (auto *editor = dynamic_cast<native::code_edit *>(owner))
                editor->on_native_focus(focused);
            break;
        }
        case KeyPress:
            {
            const KeySym symbol = XLookupKeysym(&event->xkey, 0);
            if (auto *editor =
                    dynamic_cast<native::code_edit *>(owner)) {
                const bool control =
                    (event->xkey.state & ControlMask) != 0;
                const bool extend =
                    (event->xkey.state & ShiftMask) != 0;
                if (control) {
                    if (symbol == XK_a || symbol == XK_A)
                        editor->on_native_key(
                            native::code_edit_key::select_all);
                    else if (symbol == XK_c || symbol == XK_C)
                        editor->on_native_key(
                            native::code_edit_key::copy);
                    else if (symbol == XK_x || symbol == XK_X)
                        editor->on_native_key(
                            native::code_edit_key::cut);
                    else if (symbol == XK_v || symbol == XK_V)
                        editor->on_native_key(
                            native::code_edit_key::paste);
                    else if (symbol == XK_z || symbol == XK_Z)
                        editor->on_native_key(
                            extend ? native::code_edit_key::redo
                                   : native::code_edit_key::undo);
                } else {
                    navigate(*owner, symbol);
                    const bool command =
                        symbol == XK_Left || symbol == XK_Right ||
                        symbol == XK_Up || symbol == XK_Down ||
                        symbol == XK_Home || symbol == XK_End ||
                        symbol == XK_Page_Up || symbol == XK_Page_Down ||
                        symbol == XK_BackSpace || symbol == XK_Delete ||
                        symbol == XK_Return || symbol == XK_KP_Enter ||
                        symbol == XK_Tab || symbol == XK_Escape;
                    if (!command) {
                        char text[64]{};
                        KeySym translated = NoSymbol;
                        const int count = XLookupString(
                            &event->xkey,
                            text,
                            sizeof(text),
                            &translated,
                            nullptr);
                        if (count > 0) {
                            editor->on_native_text_input(std::string(
                                text,
                                static_cast<std::size_t>(count)));
                        }
                    }
                }
                break;
            }
            navigate(*owner, symbol);
            if (auto *table =
                    dynamic_cast<native::table_view *>(owner)) {
                char text[64]{};
                KeySym translated = NoSymbol;
                const int count = XLookupString(&event->xkey,
                                                text,
                                                sizeof(text),
                                                &translated,
                                                nullptr);
                if (count > 0 &&
                    static_cast<unsigned char>(text[0]) > 0x20 &&
                    (event->xkey.state & ControlMask) == 0) {
                    table->on_native_type_text(std::string(
                        text, static_cast<std::size_t>(count)));
                }
            }
            break;
            }
        case MotionNotify:
            owner->on_mouse_move.emit(native::point(
                event->xmotion.x, event->xmotion.y));
            break;
        case ButtonPress:
        case ButtonRelease:
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_mouse_wheel.emit(native::mouse_wheel_event(
                    native::point(event->xbutton.x, event->xbutton.y),
                    event->xbutton.button == Button4 ? 24 : -24,
                    native::wheel_direction::vertical));
                break;
            }
            if (event->xbutton.button != Button1)
                break;
            if (event->type == ButtonPress)
                XmProcessTraversal(widget, XmTRAVERSE_CURRENT);
            owner->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                event->type == ButtonPress
                    ? native::mouse_action::press
                    : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            if (event->type == ButtonRelease) {
                if (auto *table =
                        dynamic_cast<native::table_view *>(owner)) {
                    auto *state = binding(*owner);
                    const auto selected = table->get_selected_rows();
                    const native::table_row_id row = selected.empty()
                        ? native::invalid_table_row_id
                        : selected.back();
                    if (state &&
                        row != native::invalid_table_row_id &&
                        state->last_row == row &&
                        event->xbutton.time - state->last_click <= 400) {
                        table->on_native_activate(row);
                        state->last_click = 0;
                        state->last_row =
                            native::invalid_table_row_id;
                    } else if (state) {
                        state->last_click = event->xbutton.time;
                        state->last_row = row;
                    }
                    break;
                }
                if (auto *tree =
                        dynamic_cast<native::tree_view *>(owner)) {
                    auto *state = binding(*owner);
                    const native::tree_item_id item = tree->item_at(
                        native::point(event->xbutton.x,
                                      event->xbutton.y));
                    if (state &&
                        item != native::invalid_tree_item_id &&
                        item == state->last_tree_item &&
                        event->xbutton.time - state->last_click <= 400) {
                        tree->on_native_double_click(item);
                        state->last_click = 0;
                        state->last_tree_item =
                            native::invalid_tree_item_id;
                    } else if (state) {
                        state->last_click = event->xbutton.time;
                        state->last_tree_item = item;
                    }
                    break;
                }
                auto *icons = dynamic_cast<native::icon_view *>(owner);
                auto *state = binding(*owner);
                if (!icons || !state)
                    break;
                const int item = icons->item_at(native::point(
                    event->xbutton.x, event->xbutton.y));
                if (item >= 0 && item == state->last_item &&
                    event->xbutton.time - state->last_click <= 400) {
                    icons->on_native_activate(item);
                    state->last_click = 0;
                    state->last_item = -1;
                } else {
                    state->last_click = event->xbutton.time;
                    state->last_item = item;
                }
            }
            break;
        }
    }

    Widget create_host(native::wnd &owner) {
        native::wnd *parent = owner.get_parent();
        Widget parent_widget = parent
                                   ? linux::openmotif::wnd_bindings
                                         .handle_from_object(parent)
                                   : nullptr;
        if (!parent || !parent->get_created() || !parent_widget)
            throw std::runtime_error(
                "Motif: collection requires a created parent.");
        const native::rect bounds = owner.get_bounds();
        Widget widget = XtVaCreateWidget(
            "collection",
            xmDrawingAreaWidgetClass,
            parent_widget,
            XmNx,
            bounds.p.x,
            XmNy,
            bounds.p.y,
            XmNwidth,
            bounds.d.w,
            XmNheight,
            bounds.d.h,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
        if (!widget)
            throw std::runtime_error(
                "Motif: failed to create collection drawing area.");
        XtAddEventHandler(widget,
                          ExposureMask | StructureNotifyMask |
                              FocusChangeMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask,
                          False,
                          handle_event,
                          &owner);
        linux::openmotif::wnd_bindings.register_pair(widget, &owner);
        return widget;
    }

    void destroy_host(native::wnd &owner,
                      linux::openmotif::motif_collection *state) {
        owner.on_native_destroy();
        if (!state)
            return;
        if (state->widget) {
            linux::openmotif::wnd_bindings.unregister_by_handle(
                state->widget);
            XtDestroyWidget(state->widget);
        }
        delete state;
    }
} // namespace

namespace linux::openmotif
{
    Widget create_collection_host(native::wnd &owner) {
        return create_host(owner);
    }

    void destroy_collection_host(native::wnd &owner,
                                 motif_collection *state) {
        destroy_host(owner, state);
    }
} // namespace linux::openmotif

namespace native
{
    void accordion::apply_items() { invalidate(); }

    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = new linux::openmotif::motif_collection();
        state->widget = create_host(*self);
        linux::openmotif::accordion_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_wnd_create.emit();
    }

    void accordion::show() const {
        auto *state = linux::openmotif::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: accordion is not created.");
        XtManageChild(state->widget);
    }

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = linux::openmotif::accordion_bindings
                          .object_from_handle(self);
        destroy_host(*self, state);
        linux::openmotif::accordion_bindings.unregister_by_handle(self);
    }

    void icon_view::apply_items() { invalidate(); }
    void icon_view::apply_icon_size() { invalidate(); }
    void icon_view::apply_label_mode() { invalidate(); }
    void icon_view::apply_selected_index() { invalidate(); }
    void icon_view::apply_scroll_offset() { invalidate(); }

    void icon_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *state = new linux::openmotif::motif_collection();
        state->widget = create_host(*self);
        linux::openmotif::icon_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void icon_view::show() const {
        auto *state = linux::openmotif::icon_view_bindings
                          .object_from_handle(
                              const_cast<icon_view *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: icon_view is not created.");
        XtManageChild(state->widget);
    }

    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *state = linux::openmotif::icon_view_bindings
                          .object_from_handle(self);
        destroy_host(*self, state);
        linux::openmotif::icon_view_bindings.unregister_by_handle(self);
    }

} // namespace native
