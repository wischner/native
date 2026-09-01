//
// Implements OPEN LOOK collection and source-editor Panel routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>

#include <native.h>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <xview/canvas.h>
#include <xview/panel.h>
#include <xview/win_input.h>
#include <xview/xview.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    linux::openlook::openlook_collection *state_for(
        native::wnd &owner) {
        if (auto *accordion = dynamic_cast<native::accordion *>(&owner))
            return linux::openlook::accordion_bindings
                .object_from_handle(accordion);
        if (auto *icons = dynamic_cast<native::icon_view *>(&owner))
            return linux::openlook::icon_view_bindings
                .object_from_handle(icons);
        if (auto *tree = dynamic_cast<native::tree_view *>(&owner))
            return linux::openlook::tree_view_bindings
                .object_from_handle(tree);
        if (auto *table = dynamic_cast<native::table_view *>(&owner))
            return linux::openlook::table_view_bindings
                .object_from_handle(table);
        if (auto *editor = dynamic_cast<native::code_edit *>(&owner))
            return linux::openlook::code_edit_bindings
                .object_from_handle(editor);
        return nullptr;
    }

    void ensure_backbuffer(native::wnd &owner, int width, int height) {
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(&owner);
        if (!cache || width <= 0 || height <= 0)
            return;
        if (cache->backbuffer && cache->buffer_width == width &&
            cache->buffer_height == height)
            return;
        if (cache->backbuffer)
            XFreePixmap(linux::openlook::cached_display,
                        cache->backbuffer);
        const Window window = linux::openlook::drawable(&owner);
        if (window == None)
            return;
        cache->backbuffer = XCreatePixmap(
            linux::openlook::cached_display,
            window,
            width,
            height,
            DefaultDepth(linux::openlook::cached_display,
                         DefaultScreen(linux::openlook::cached_display)));
        cache->buffer_width = width;
        cache->buffer_height = height;
    }

    void repaint(Panel panel,
                 Xv_Window paint_window,
                 Rectlist *areas) {
        auto *owner = linux::openlook::wnd_bindings
                          .object_from_handle(panel);
        if (!owner || !owner->get_created())
            return;
        const int width = static_cast<int>(xv_get(panel, XV_WIDTH));
        const int height = static_cast<int>(xv_get(panel, XV_HEIGHT));
        native::rect invalid(0, 0, width, height);
        if (areas && !rl_empty(areas)) {
            invalid = native::rect(areas->rl_bound.r_left + areas->rl_x,
                                   areas->rl_bound.r_top + areas->rl_y,
                                   areas->rl_bound.r_width,
                                   areas->rl_bound.r_height);
        }
        auto &graphics = owner->get_gpx();
        ensure_backbuffer(*owner, width, height);
        graphics.set_clip(invalid);
        native::wnd_paint_event event(invalid, graphics);
        owner->on_wnd_paint.emit(event);
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(owner);
        const Window window = paint_window
                                  ? static_cast<Window>(xv_get(
                                        paint_window, XV_XID))
                                  : None;
        if (!cache || !cache->gc || !cache->backbuffer ||
            window == None)
            return;
        XSetClipMask(linux::openlook::cached_display, cache->gc, None);
        XCopyArea(linux::openlook::cached_display,
                  cache->backbuffer,
                  window,
                  cache->gc,
                  invalid.p.x,
                  invalid.p.y,
                  invalid.d.w,
                  invalid.d.h,
                  invalid.p.x,
                  invalid.p.y);
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

    Notify_value handle_event(Xv_Window window,
                              Event *event,
                              Notify_arg argument,
                              Notify_event_type type) {
        auto *owner = reinterpret_cast<native::wnd *>(
            linux::openlook::collection_paint_bindings
                .object_from_handle(window));
        if (!owner || !event)
            return notify_next_event_func(
                window,
                reinterpret_cast<Notify_event>(event),
                argument,
                type);
        const int action = event_action(event);
        if (action == KBD_USE) {
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(owner))
                accordion->on_native_focus(true);
            if (auto *icons = dynamic_cast<native::icon_view *>(owner))
                icons->on_native_focus(true);
            if (auto *tree = dynamic_cast<native::tree_view *>(owner))
                tree->on_native_focus(true);
            if (auto *table = dynamic_cast<native::table_view *>(owner))
                table->on_native_focus(true);
            if (auto *editor = dynamic_cast<native::code_edit *>(owner))
                editor->on_native_focus(true);
        } else if (action == KBD_DONE) {
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(owner))
                accordion->on_native_focus(false);
            if (auto *icons = dynamic_cast<native::icon_view *>(owner))
                icons->on_native_focus(false);
            if (auto *tree = dynamic_cast<native::tree_view *>(owner))
                tree->on_native_focus(false);
            if (auto *table = dynamic_cast<native::table_view *>(owner))
                table->on_native_focus(false);
            if (auto *editor = dynamic_cast<native::code_edit *>(owner))
                editor->on_native_focus(false);
        } else if (action == ACTION_SCROLL_UP ||
                   action == ACTION_SCROLL_DOWN) {
            owner->on_mouse_wheel.emit(native::mouse_wheel_event(
                native::point(event_x(event), event_y(event)),
                action == ACTION_SCROLL_UP ? 24 : -24,
                native::wheel_direction::vertical));
        } else if (action == ACTION_SELECT) {
            if (event_is_down(event))
                xv_set(window, WIN_SET_FOCUS, nullptr);
            owner->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                event_is_down(event) ? native::mouse_action::press
                                     : native::mouse_action::release,
                native::point(event_x(event), event_y(event))));
            if (!event_is_down(event)) {
                if (auto *table =
                        dynamic_cast<native::table_view *>(owner)) {
                    auto *state = state_for(*owner);
                    XEvent *native_event = event_xevent(event);
                    const Time time = native_event
                        ? native_event->xbutton.time
                        : CurrentTime;
                    const auto selected = table->get_selected_rows();
                    const native::table_row_id row = selected.empty()
                        ? native::invalid_table_row_id
                        : selected.back();
                    if (state && time != CurrentTime &&
                        row != native::invalid_table_row_id &&
                        state->last_row == row &&
                        time - state->last_click <= 400) {
                        table->on_native_activate(row);
                        state->last_click = 0;
                        state->last_row =
                            native::invalid_table_row_id;
                    } else if (state) {
                        state->last_click = time;
                        state->last_row = row;
                    }
                }
                if (auto *tree =
                        dynamic_cast<native::tree_view *>(owner)) {
                    auto *state = state_for(*owner);
                    XEvent *native_event = event_xevent(event);
                    const Time time = native_event
                                          ? native_event->xbutton.time
                                          : CurrentTime;
                    const native::tree_view_hit hit = tree->hit_test(
                        native::point(event_x(event), event_y(event)));
                    const native::tree_item_id item = hit.id;
                    if (state && time != CurrentTime &&
                        hit.part == native::tree_view_hit_part::row &&
                        item != native::invalid_tree_item_id &&
                        item == state->last_tree_item &&
                        time - state->last_click <= 400) {
                        tree->on_native_double_click(item);
                        state->last_click = 0;
                        state->last_tree_item =
                            native::invalid_tree_item_id;
                    } else if (state) {
                        state->last_click = time;
                        state->last_tree_item =
                            hit.part == native::tree_view_hit_part::row
                                ? item
                                : native::invalid_tree_item_id;
                    }
                }
                auto *icons = dynamic_cast<native::icon_view *>(owner);
                auto *state = state_for(*owner);
                if (icons && state) {
                    XEvent *native_event = event_xevent(event);
                    const Time time = native_event
                                          ? native_event->xbutton.time
                                          : CurrentTime;
                    const int item = icons->item_at(native::point(
                        event_x(event), event_y(event)));
                    if (item >= 0 && item == state->last_item &&
                        time != CurrentTime &&
                        time - state->last_click <= 400) {
                        icons->on_native_activate(item);
                        state->last_click = 0;
                        state->last_item = -1;
                    } else {
                        state->last_click = time;
                        state->last_item = item;
                    }
                }
            }
        }
        XEvent *native_event = event_xevent(event);
        if (native_event && native_event->type == MotionNotify) {
            owner->on_mouse_move.emit(native::point(
                native_event->xmotion.x, native_event->xmotion.y));
        }
        if (native_event && native_event->type == KeyPress) {
            const KeySym symbol =
                XLookupKeysym(&native_event->xkey, 0);
            if (auto *editor =
                    dynamic_cast<native::code_edit *>(owner)) {
                const bool control =
                    (native_event->xkey.state & ControlMask) != 0;
                const bool extend =
                    (native_event->xkey.state & ShiftMask) != 0;
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
                            &native_event->xkey,
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
            } else {
                navigate(*owner, symbol);
            }
            if (auto *table =
                    dynamic_cast<native::table_view *>(owner)) {
                char text[64]{};
                KeySym translated = NoSymbol;
                const int count = XLookupString(
                    &native_event->xkey,
                    text,
                    sizeof(text),
                    &translated,
                    nullptr);
                if (count > 0 &&
                    static_cast<unsigned char>(text[0]) > 0x20 &&
                    (native_event->xkey.state & ControlMask) == 0) {
                    table->on_native_type_text(std::string(
                        text, static_cast<std::size_t>(count)));
                }
            }
        }
        return notify_next_event_func(
            window,
            reinterpret_cast<Notify_event>(event),
            argument,
            type);
    }

    linux::openlook::openlook_collection *create_panel(
        native::wnd &owner) {
        native::point position = owner.get_position();
        native::wnd *root = owner.get_parent();
        while (root && !dynamic_cast<native::app_wnd *>(root)) {
            position.x = static_cast<native::coord>(
                position.x + root->get_position().x);
            position.y = static_cast<native::coord>(
                position.y + root->get_position().y);
            root = root->get_parent();
        }
        auto *window = dynamic_cast<native::app_wnd *>(root);
        auto *window_state =
            window ? linux::openlook::window_state(window) : nullptr;
        if (!window_state || !window_state->frame) {
            throw std::runtime_error(
                "OpenLook/XView: collection requires a created "
                "top-level parent.");
        }
        const native::rect bounds = owner.get_bounds();
        Panel panel = static_cast<Panel>(xv_create(
            window_state->frame,
            PANEL,
            PANEL_LAYOUT,
            PANEL_HORIZONTAL,
            PANEL_BORDER,
            FALSE,
            PANEL_REPAINT_PROC,
            repaint,
            XV_X,
            position.x,
            XV_Y,
            position.y + window_state->menu_height,
            XV_WIDTH,
            bounds.d.w,
            XV_HEIGHT,
            bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
        if (!panel)
            throw std::runtime_error(
                "OpenLook/XView: failed to create collection panel.");
        Xv_Window paint_window = static_cast<Xv_Window>(xv_get(
            panel, CANVAS_NTH_PAINT_WINDOW, 0));
        if (!paint_window) {
            xv_destroy_safe(panel);
            throw std::runtime_error(
                "OpenLook/XView: collection has no paint window.");
        }
        linux::openlook::wnd_bindings.register_pair(panel, &owner);
        linux::openlook::collection_paint_bindings.register_pair(
            paint_window, &owner);
        xv_set(paint_window,
               WIN_NOTIFY_SAFE_EVENT_PROC,
               handle_event,
               WIN_CONSUME_EVENTS,
               KBD_USE,
               KBD_DONE,
               WIN_ASCII_EVENTS,
               WIN_LEFT_KEYS,
               WIN_RIGHT_KEYS,
               LOC_MOVE,
               ACTION_SELECT,
               ACTION_SCROLL_UP,
               ACTION_SCROLL_DOWN,
               nullptr,
               nullptr);
        auto *state = new linux::openlook::openlook_collection();
        state->panel = panel;
        state->paint_window = paint_window;
        return state;
    }

    void destroy_panel(native::wnd &owner,
                       linux::openlook::openlook_collection *state) {
        owner.on_native_destroy();
        if (!state)
            return;
        if (state->panel) {
            linux::openlook::collection_paint_bindings
                .unregister_by_handle(state->paint_window);
            linux::openlook::wnd_bindings.unregister_by_handle(
                state->panel);
            xv_destroy_safe(state->panel);
        }
        delete state;
    }
} // namespace

namespace linux::openlook
{
    openlook_collection *create_collection_panel(native::wnd &owner) {
        return create_panel(owner);
    }

    void destroy_collection_panel(native::wnd &owner,
                                  openlook_collection *state) {
        destroy_panel(owner, state);
    }
} // namespace linux::openlook

namespace native
{
    void accordion::apply_items() { invalidate(); }
    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = create_panel(*self);
        linux::openlook::accordion_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_wnd_create.emit();
    }
    void accordion::show() const {
        auto *state = linux::openlook::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: accordion is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }
    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = linux::openlook::accordion_bindings
                          .object_from_handle(self);
        destroy_panel(*self, state);
        linux::openlook::accordion_bindings.unregister_by_handle(self);
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
        auto *state = create_panel(*self);
        linux::openlook::icon_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }
    void icon_view::show() const {
        auto *state = linux::openlook::icon_view_bindings
                          .object_from_handle(
                              const_cast<icon_view *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: icon_view is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
    }
    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *state = linux::openlook::icon_view_bindings
                          .object_from_handle(self);
        destroy_panel(*self, state);
        linux::openlook::icon_view_bindings.unregister_by_handle(self);
    }

} // namespace native
