//
// Implements OPEN LOOK collection and source-editor Panel routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>
#include <limits>

#include <native.h>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <xview/canvas.h>
#include <xview/panel.h>
#include <xview/scrollbar.h>
#include <xview/win_input.h>
#include <xview/xview.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    Notify_value handle_scrollbar_event(
        Notify_client client,
        Event *event,
        Scrollbar scrollbar,
        Notify_event_type type);

    linux::openlook::openlook_collection *state_for(
        native::wnd &owner) {
        if (auto *accordion = dynamic_cast<native::accordion *>(&owner))
            return linux::openlook::accordion_bindings
                .object_from_handle(accordion);
        if (auto *tabs = dynamic_cast<native::tab_view *>(&owner))
            return linux::openlook::tab_view_bindings
                .object_from_handle(tabs);
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

    int saturated_int(std::size_t value) {
        return value > static_cast<std::size_t>(
                           std::numeric_limits<int>::max())
                   ? std::numeric_limits<int>::max()
                   : static_cast<int>(value);
    }

    void configure_scrollbar(Scrollbar scrollbar,
                             bool visible,
                             int object_length,
                             int page_length,
                             int view_start) {
        if (!scrollbar)
            return;
        const int object = std::max(1, object_length);
        const int page = std::clamp(page_length, 1, object);
        const int start = std::clamp(
            view_start, 0, std::max(0, object - page));
        xv_set(scrollbar,
               SCROLLBAR_OBJECT_LENGTH,
               object,
               SCROLLBAR_PAGE_LENGTH,
               page,
               SCROLLBAR_VIEW_LENGTH,
               page,
               SCROLLBAR_VIEW_START,
               start,
               SCROLLBAR_INACTIVE,
               visible ? FALSE : TRUE,
               XV_SHOW,
               visible ? TRUE : FALSE,
               nullptr);
    }

    void place_scrollbar(Scrollbar scrollbar,
                         int x,
                         int y,
                         int width,
                         int height) {
        if (!scrollbar)
            return;
        xv_set(scrollbar,
               XV_X,
               std::max(0, x),
               XV_Y,
               std::max(0, y),
               XV_WIDTH,
               std::max(1, width),
               XV_HEIGHT,
               std::max(1, height),
               nullptr);
    }

    void synchronize_scrollbars(
        native::wnd &owner,
        linux::openlook::openlook_collection &state) {
        if (state.synchronizing_scrollbars)
            return;
        state.synchronizing_scrollbars = true;

        const native::size dimensions = owner.get_dimensions();
        const int width = std::max(1, static_cast<int>(dimensions.w));
        const int height = std::max(1, static_cast<int>(dimensions.h));
        const int scrollbar_extent = std::max(
            1, scrollbar_width_for_scale(WIN_SCALE_MEDIUM));

        if (auto *icons = dynamic_cast<native::icon_view *>(&owner)) {
            const int object = std::max(
                1, static_cast<int>(icons->get_content_dimensions().h));
            configure_scrollbar(state.vertical_scrollbar,
                                object > height,
                                object,
                                height,
                                icons->get_scroll_offset());
            place_scrollbar(state.vertical_scrollbar,
                            width - scrollbar_extent,
                            0,
                            scrollbar_extent,
                            height);
        } else if (auto *tree =
                       dynamic_cast<native::tree_view *>(&owner)) {
            const std::size_t count = tree->get_visible_item_count();
            const int row_height = count > 0
                                       ? std::max<int>(
                                             1,
                                             tree->get_row_bounds(0).d.h)
                                       : 1;
            const std::size_t maximum =
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max()) /
                static_cast<std::size_t>(row_height);
            const int object = count > maximum
                                   ? std::numeric_limits<int>::max()
                                   : std::max(
                                         1,
                                         static_cast<int>(count) *
                                             row_height);
            configure_scrollbar(state.vertical_scrollbar,
                                object > height,
                                object,
                                height,
                                tree->get_scroll_offset());
            place_scrollbar(state.vertical_scrollbar,
                            width - scrollbar_extent,
                            0,
                            scrollbar_extent,
                            height);
        } else if (auto *table =
                       dynamic_cast<native::table_view *>(&owner)) {
            auto appearance = native::theme::create(owner.get_gpx());
            const native::theme::metrics metrics =
                appearance->defaults();
            const int header = table->get_header_visible()
                                   ? std::max(0, metrics.header_height)
                                   : 0;
            const int row = std::max(
                1,
                table->get_row_height()
                    ? static_cast<int>(*table->get_row_height())
                    : metrics.table_row_height);
            const int total_rows = std::max(
                1, saturated_int(table->get_display_row_count()));
            int content_width = 0;
            for (const native::table_column &column :
                table->get_columns()) {
                if (column.visible) {
                    const int column_width =
                        static_cast<int>(column.width);
                    content_width =
                        content_width >
                                std::numeric_limits<int>::max() -
                                    column_width
                            ? std::numeric_limits<int>::max()
                            : content_width + column_width;
                }
            }
            content_width = std::max(1, content_width);
            int body_width = width;
            int body_height = std::max(0, height - header);
            const bool needs_vertical =
                static_cast<std::uint64_t>(
                    table->get_display_row_count()) *
                    static_cast<std::uint64_t>(row) >
                static_cast<std::uint64_t>(body_height);
            const bool vertical =
                table->get_vertical_scrollbar_policy() ==
                    native::scrollbar_policy::always ||
                (table->get_vertical_scrollbar_policy() ==
                     native::scrollbar_policy::automatic &&
                 needs_vertical);
            if (vertical) {
                body_width = std::max(
                    0, body_width - scrollbar_extent);
            }
            const bool horizontal =
                table->get_horizontal_scrollbar_policy() ==
                    native::scrollbar_policy::always ||
                (table->get_horizontal_scrollbar_policy() ==
                     native::scrollbar_policy::automatic &&
                 content_width > body_width);
            if (horizontal) {
                body_height = std::max(
                    0, body_height - scrollbar_extent);
            }
            const int page_rows = std::max(1, body_height / row);
            configure_scrollbar(
                state.vertical_scrollbar,
                vertical,
                total_rows,
                page_rows,
                saturated_int(table->get_vertical_scroll_row()));
            configure_scrollbar(
                state.horizontal_scrollbar,
                horizontal,
                content_width,
                std::max(1, body_width),
                table->get_horizontal_scroll_offset());
            place_scrollbar(state.vertical_scrollbar,
                            body_width,
                            header,
                            scrollbar_extent,
                            body_height);
            place_scrollbar(state.horizontal_scrollbar,
                            0,
                            header + body_height,
                            body_width,
                            scrollbar_extent);
        }

        state.synchronizing_scrollbars = false;
    }

    void paint_and_copy(native::wnd &owner,
                        linux::openlook::openlook_collection &state,
                        const native::rect &requested) {
        if (!owner.get_created() || !state.panel ||
            !state.paint_window) {
            return;
        }
        synchronize_scrollbars(owner, state);
        const int width = static_cast<int>(xv_get(
            state.panel, XV_WIDTH));
        const int height = static_cast<int>(xv_get(
            state.panel, XV_HEIGHT));
        const native::rect invalid = requested.intersect(
            native::rect(0, 0, width, height));
        if (invalid.d.w <= 0 || invalid.d.h <= 0)
            return;

        auto &graphics = owner.get_gpx();
        ensure_backbuffer(owner, width, height);
        graphics.set_clip(invalid);
        owner.on_native_paint(native::wnd_paint_event(
            invalid, graphics));

        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(&owner);
        const Window window = static_cast<Window>(xv_get(
            state.paint_window, XV_XID));
        if (!cache || !cache->gc || !cache->backbuffer ||
            window == None) {
            return;
        }
        XSetClipMask(linux::openlook::cached_display,
                     cache->gc,
                     None);
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
        XFlush(linux::openlook::cached_display);
    }

    Notify_value handle_scrollbar_event(
        Notify_client client,
        Event *event,
        Scrollbar scrollbar,
        Notify_event_type type) {
        auto *owner = reinterpret_cast<native::wnd *>(
            linux::openlook::collection_paint_bindings
                .object_from_handle(client));
        auto *state = owner ? state_for(*owner) : nullptr;
        if (owner && state && event &&
            !state->synchronizing_scrollbars &&
            event_id(event) == SCROLLBAR_REQUEST) {
            const int start = static_cast<int>(xv_get(
                scrollbar, SCROLLBAR_VIEW_START));
            if (scrollbar == state->vertical_scrollbar) {
                if (auto *icons =
                        dynamic_cast<native::icon_view *>(owner)) {
                    icons->set_scroll_offset(start);
                } else if (auto *tree =
                               dynamic_cast<native::tree_view *>(owner)) {
                    tree->set_scroll_offset(start);
                } else if (auto *table =
                               dynamic_cast<native::table_view *>(owner)) {
                    table->on_native_scroll(
                        static_cast<std::size_t>(std::max(0, start)),
                        table->get_horizontal_scroll_offset());
                }
            } else if (scrollbar == state->horizontal_scrollbar) {
                if (auto *table =
                        dynamic_cast<native::table_view *>(owner)) {
                    table->on_native_scroll(
                        table->get_vertical_scroll_row(),
                        std::max(0, start));
                }
            }
        }
        return notify_next_event_func(
            client,
            reinterpret_cast<Notify_event>(event),
            scrollbar,
            type);
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
        auto *state = state_for(*owner);
        if (!state)
            return;
        // XView supplies the same paint window held by the collection
        // state. Keep it synchronized in case a Canvas recreates it.
        state->paint_window = paint_window;
        paint_and_copy(*owner, *state, invalid);
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
            owner->on_native_mouse_wheel(native::mouse_wheel_event(
                native::point(event_x(event), event_y(event)),
                action == ACTION_SCROLL_UP ? 24 : -24,
                native::wheel_direction::vertical));
        } else if (action == ACTION_SELECT) {
            if (event_is_down(event))
                xv_set(window, WIN_SET_FOCUS, nullptr);
            owner->on_native_mouse_click(native::mouse_event(
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
            owner->on_native_mouse_move(native::point(
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

    void configure_paint_window(
        native::wnd &owner,
        linux::openlook::openlook_collection &state,
        Xv_Window paint_window) {
        if (!paint_window)
            throw std::runtime_error(
                "OpenLook/XView: collection has no paint window.");
        if (state.paint_window && state.paint_window != paint_window) {
            linux::openlook::collection_paint_bindings
                .unregister_by_handle(state.paint_window);
        }
        state.paint_window = paint_window;
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
               LOC_DRAG,
               WIN_MOUSE_BUTTONS,
               ACTION_SELECT,
               ACTION_SCROLL_UP,
               ACTION_SCROLL_DOWN,
               nullptr,
               nullptr);
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
        auto *state = new linux::openlook::openlook_collection();
        state->panel = panel;
        try {
            linux::openlook::wnd_bindings.register_pair(panel, &owner);
            configure_paint_window(owner, *state, paint_window);
            const bool vertical =
                dynamic_cast<native::icon_view *>(&owner) ||
                dynamic_cast<native::tree_view *>(&owner) ||
                dynamic_cast<native::table_view *>(&owner);
            if (vertical) {
                state->vertical_scrollbar =
                    static_cast<Scrollbar>(xv_create(
                        panel,
                        SCROLLBAR,
                        SCROLLBAR_DIRECTION,
                        SCROLLBAR_VERTICAL,
                        SCROLLBAR_PIXELS_PER_UNIT,
                        1,
                        SCROLLBAR_SPLITTABLE,
                        FALSE,
                        XV_SHOW,
                        FALSE,
                        nullptr));
                if (!state->vertical_scrollbar) {
                    throw std::runtime_error(
                        "OpenLook/XView: failed to create native "
                        "vertical scrollbar.");
                }
            }
            if (dynamic_cast<native::table_view *>(&owner)) {
                state->horizontal_scrollbar =
                    static_cast<Scrollbar>(xv_create(
                        panel,
                        SCROLLBAR,
                        SCROLLBAR_DIRECTION,
                        SCROLLBAR_HORIZONTAL,
                        SCROLLBAR_PIXELS_PER_UNIT,
                        1,
                        SCROLLBAR_SPLITTABLE,
                        FALSE,
                        XV_SHOW,
                        FALSE,
                        nullptr));
                if (!state->horizontal_scrollbar) {
                    throw std::runtime_error(
                        "OpenLook/XView: failed to create native "
                        "horizontal scrollbar.");
                }
            }

            Notify_client installed_client = XV_NULL;
            for (Scrollbar scrollbar : {
                     state->vertical_scrollbar,
                     state->horizontal_scrollbar}) {
                if (!scrollbar)
                    continue;
                Notify_client client = static_cast<Notify_client>(
                    xv_get(scrollbar, SCROLLBAR_NOTIFY_CLIENT));
                if (!client || client == installed_client)
                    continue;
                if (notify_interpose_event_func(
                        client,
                        reinterpret_cast<Notify_func>(
                            handle_scrollbar_event),
                        NOTIFY_SAFE) != NOTIFY_OK) {
                    throw std::runtime_error(
                        "OpenLook/XView: failed to monitor native "
                        "scrollbar.");
                }
                installed_client = client;
            }
        } catch (...) {
            delete state;
            linux::openlook::collection_paint_bindings
                .unregister_by_handle(paint_window);
            linux::openlook::wnd_bindings.unregister_by_handle(panel);
            xv_destroy_safe(panel);
            throw;
        }
        return state;
    }

    void destroy_panel(native::wnd &owner,
                       linux::openlook::openlook_collection *state) {
        owner.on_native_destroy();
        if (!state)
            return;
        if (state->panel) {
            if (state->content_panel)
                xv_destroy_safe(state->content_panel);
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

    void repaint_collection(native::wnd &owner,
                            const native::rect &area) {
        openlook_collection *state = state_for(owner);
        if (state)
            paint_and_copy(owner, *state, area);
    }

    void resize_collection_panel(native::wnd &owner,
                                 openlook_collection &state,
                                 const native::size &dimensions) {
        if (!state.panel)
            return;
        xv_set(state.panel,
               XV_WIDTH,
               dimensions.w,
               XV_HEIGHT,
               dimensions.h,
               nullptr);
        if (state.content_panel) {
            const auto *tabs = dynamic_cast<native::tab_view *>(&owner);
            const native::rect content = tabs
                ? tabs->get_content_bounds()
                : native::rect(0, 0, dimensions.w, dimensions.h);
            const int panel_x = static_cast<int>(xv_get(
                state.panel, XV_X));
            const int panel_y = static_cast<int>(xv_get(
                state.panel, XV_Y));
            xv_set(state.content_panel,
                   XV_X, panel_x + content.p.x,
                   XV_Y, panel_y + content.p.y,
                   XV_WIDTH, content.d.w,
                   XV_HEIGHT, content.d.h,
                   nullptr);
        }
        configure_paint_window(
            owner,
            state,
            static_cast<Xv_Window>(xv_get(
                state.panel, CANVAS_NTH_PAINT_WINDOW, 0)));
        synchronize_scrollbars(owner, state);
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
        self->on_native_create();
    }
    void accordion::show() const {
        auto *state = linux::openlook::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: accordion is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
        const Window window = static_cast<Window>(
            xv_get(state->panel, XV_XID));
        if (window != None && linux::openlook::cached_display) {
            XRaiseWindow(linux::openlook::cached_display, window);
            XFlush(linux::openlook::cached_display);
        }
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            accordion_item &item = get_item(index);
            if (item.get_expanded() && item.get_content().get_created())
                item.get_content().show();
        }
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
        self->on_native_create();
    }
    void icon_view::show() const {
        auto *state = linux::openlook::icon_view_bindings
                          .object_from_handle(
                              const_cast<icon_view *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: icon_view is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
        const Window window = static_cast<Window>(
            xv_get(state->panel, XV_XID));
        if (window != None && linux::openlook::cached_display) {
            XRaiseWindow(linux::openlook::cached_display, window);
            XFlush(linux::openlook::cached_display);
        }
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
