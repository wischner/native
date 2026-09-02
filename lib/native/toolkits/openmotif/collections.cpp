//
// Implements Motif collection and source-editor host event routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <Xm/DrawingA.h>
#include <Xm/ScrollBar.h>

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

    int saturated_int(std::size_t value) {
        return value > static_cast<std::size_t>(
                           std::numeric_limits<int>::max())
                   ? std::numeric_limits<int>::max()
                   : static_cast<int>(value);
    }

    void configure_scrollbar(Widget scrollbar,
                             bool visible,
                             int x,
                             int y,
                             int width,
                             int height,
                             int total,
                             int page,
                             int value,
                             int increment) {
        if (!scrollbar)
            return;
        if (!visible) {
            if (XtIsManaged(scrollbar))
                XtUnmanageChild(scrollbar);
            return;
        }
        const int object = std::max(1, total);
        const int slider = std::clamp(page, 1, object);
        const int position = std::clamp(
            value, 0, std::max(0, object - slider));
        XtVaSetValues(scrollbar,
                      XmNx,
                      std::max(0, x),
                      XmNy,
                      std::max(0, y),
                      XmNwidth,
                      std::max(1, width),
                      XmNheight,
                      std::max(1, height),
                      XmNminimum,
                      0,
                      XmNmaximum,
                      object,
                      XmNvalue,
                      position,
                      XmNsliderSize,
                      slider,
                      XmNincrement,
                      std::clamp(increment, 1, object),
                      XmNpageIncrement,
                      slider,
                      nullptr);
        if (!XtIsManaged(scrollbar))
            XtManageChild(scrollbar);
        if (XtIsRealized(scrollbar))
            XRaiseWindow(linux::openmotif::cached_display,
                         XtWindow(scrollbar));
    }

    void synchronize_scrollbars(
        native::wnd &owner,
        linux::openmotif::motif_collection &state) {
        if (state.synchronizing_scrollbars ||
            (!state.vertical_scrollbar &&
             !state.horizontal_scrollbar)) {
            return;
        }
        state.synchronizing_scrollbars = true;
        const native::rect bounds = owner.get_bounds();
        const int width = std::max(1, static_cast<int>(bounds.d.w));
        const int height = std::max(1, static_cast<int>(bounds.d.h));
        int extent = 16;
        int header = 0;
        int row_height = 20;
        if (owner.get_created()) {
            auto appearance = native::theme::create(owner.get_gpx());
            const native::theme::metrics metrics = appearance->defaults();
            extent = std::max(1, metrics.scrollbar_extent);
            header = metrics.header_height;
            row_height = std::max(1, metrics.table_row_height);
        }

        if (auto *icons = dynamic_cast<native::icon_view *>(&owner)) {
            const int total = std::max(
                1,
                static_cast<int>(icons->get_content_dimensions().h));
            configure_scrollbar(state.vertical_scrollbar,
                                total > height,
                                bounds.p.x + width - extent,
                                bounds.p.y,
                                extent,
                                height,
                                total,
                                height,
                                icons->get_scroll_offset(),
                                24);
        } else if (auto *tree =
                       dynamic_cast<native::tree_view *>(&owner)) {
            const std::size_t count = tree->get_visible_item_count();
            const int row = count > 0
                                ? std::max<int>(
                                      1,
                                      tree->get_row_bounds(0).d.h)
                                : 1;
            const std::size_t limit =
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max()) /
                static_cast<std::size_t>(row);
            const int total = count > limit
                                  ? std::numeric_limits<int>::max()
                                  : std::max(
                                        1,
                                        static_cast<int>(count) * row);
            configure_scrollbar(state.vertical_scrollbar,
                                total > height,
                                bounds.p.x + width - extent,
                                bounds.p.y,
                                extent,
                                height,
                                total,
                                height,
                                tree->get_scroll_offset(),
                                row);
        } else if (auto *table =
                       dynamic_cast<native::table_view *>(&owner)) {
            header = table->get_header_visible() ? header : 0;
            row_height = table->get_row_height()
                             ? std::max<int>(
                                   1, *table->get_row_height())
                             : row_height;
            int content_width = 0;
            for (const native::table_column &column :
                 table->get_columns()) {
                if (!column.visible)
                    continue;
                const int column_width = column.width;
                content_width = content_width >
                                        std::numeric_limits<int>::max() -
                                            column_width
                                    ? std::numeric_limits<int>::max()
                                    : content_width + column_width;
            }
            content_width = std::max(1, content_width);
            int body_width = width;
            int body_height = std::max(0, height - header);
            const std::size_t rows = table->get_display_row_count();
            const bool needs_vertical =
                rows > static_cast<std::size_t>(
                           std::max(0, body_height) / row_height);
            const bool vertical =
                table->get_vertical_scrollbar_policy() ==
                    native::scrollbar_policy::always ||
                (table->get_vertical_scrollbar_policy() ==
                     native::scrollbar_policy::automatic &&
                 needs_vertical);
            if (vertical)
                body_width = std::max(0, body_width - extent);
            const bool needs_horizontal = content_width > body_width;
            const bool horizontal =
                table->get_horizontal_scrollbar_policy() ==
                    native::scrollbar_policy::always ||
                (table->get_horizontal_scrollbar_policy() ==
                     native::scrollbar_policy::automatic &&
                 needs_horizontal);
            if (horizontal)
                body_height = std::max(0, body_height - extent);
            const int page_rows = std::max(
                1, body_height / row_height);
            configure_scrollbar(
                state.vertical_scrollbar,
                vertical,
                bounds.p.x + body_width,
                bounds.p.y + header,
                extent,
                body_height,
                std::max(1, saturated_int(rows)),
                page_rows,
                saturated_int(table->get_vertical_scroll_row()),
                1);
            configure_scrollbar(
                state.horizontal_scrollbar,
                horizontal,
                bounds.p.x,
                bounds.p.y + header + body_height,
                body_width,
                extent,
                content_width,
                std::max(1, body_width),
                table->get_horizontal_scroll_offset(),
                20);
        }
        state.synchronizing_scrollbars = false;
    }

    void scrollbar_changed(Widget widget,
                           XtPointer client_data,
                           XtPointer call_data) {
        auto *owner = static_cast<native::wnd *>(client_data);
        auto *scroll = static_cast<XmScrollBarCallbackStruct *>(call_data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !scroll || !state ||
            state->synchronizing_scrollbars) {
            return;
        }
        if (widget == state->vertical_scrollbar) {
            if (auto *icons = dynamic_cast<native::icon_view *>(owner))
                icons->set_scroll_offset(scroll->value);
            else if (auto *tree =
                         dynamic_cast<native::tree_view *>(owner))
                tree->set_scroll_offset(scroll->value);
            else if (auto *table =
                         dynamic_cast<native::table_view *>(owner)) {
                table->on_native_scroll(
                    static_cast<std::size_t>(std::max(0, scroll->value)),
                    table->get_horizontal_scroll_offset());
            }
        } else if (widget == state->horizontal_scrollbar) {
            if (auto *table =
                    dynamic_cast<native::table_view *>(owner)) {
                table->on_native_scroll(
                    table->get_vertical_scroll_row(),
                    std::max(0, scroll->value));
            }
        }
    }

    Widget create_scrollbar(Widget parent,
                            native::wnd &owner,
                            unsigned char orientation) {
        Widget result = XtVaCreateWidget(
            orientation == XmVERTICAL
                ? "collectionVerticalScroll"
                : "collectionHorizontalScroll",
            xmScrollBarWidgetClass,
            parent,
            XmNorientation,
            orientation,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
        XtAddCallback(result,
                      XmNvalueChangedCallback,
                      scrollbar_changed,
                      &owner);
        XtAddCallback(result,
                      XmNdragCallback,
                      scrollbar_changed,
                      &owner);
        return result;
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
        if (auto *state = binding(owner))
            synchronize_scrollbars(owner, *state);
        resize_backbuffer(owner, widget, width, height);
        native::rect invalid(0, 0, width, height);
        graphics.set_clip(invalid);
        native::wnd_paint_event event(invalid, graphics);
        owner.on_native_paint(event);
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
            if (auto *state = binding(*owner))
                synchronize_scrollbars(*owner, *state);
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
            owner->on_native_mouse_move(native::point(
                event->xmotion.x, event->xmotion.y));
            break;
        case ButtonPress:
        case ButtonRelease:
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_native_mouse_wheel(native::mouse_wheel_event(
                    native::point(event->xbutton.x, event->xbutton.y),
                    event->xbutton.button == Button4 ? 24 : -24,
                    native::wheel_direction::vertical));
                break;
            }
            if (event->xbutton.button != Button1)
                break;
            if (event->type == ButtonPress)
                XmProcessTraversal(widget, XmTRAVERSE_CURRENT);
            owner->on_native_mouse_click(native::mouse_event(
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

    Widget create_host(
        native::wnd &owner,
        linux::openmotif::motif_collection &state) {
        native::wnd *parent = owner.get_parent();
        Widget parent_widget = linux::openmotif::parent_widget(&owner);
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
            XmNresizePolicy,
            XmRESIZE_NONE,
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
        if (dynamic_cast<native::icon_view *>(&owner) ||
            dynamic_cast<native::tree_view *>(&owner) ||
            dynamic_cast<native::table_view *>(&owner)) {
            state.vertical_scrollbar = create_scrollbar(
                parent_widget, owner, XmVERTICAL);
        }
        if (dynamic_cast<native::table_view *>(&owner)) {
            state.horizontal_scrollbar = create_scrollbar(
                parent_widget, owner, XmHORIZONTAL);
        }
        return widget;
    }

    void destroy_host(native::wnd &owner,
                      linux::openmotif::motif_collection *state) {
        owner.on_native_destroy();
        if (!state)
            return;
        linux::openmotif::destroy_collection_scrollbars(*state);
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
    Widget create_collection_host(native::wnd &owner,
                                  motif_collection &state) {
        return create_host(owner, state);
    }

    void destroy_collection_scrollbars(motif_collection &state) {
        if (state.vertical_scrollbar)
            XtDestroyWidget(state.vertical_scrollbar);
        if (state.horizontal_scrollbar)
            XtDestroyWidget(state.horizontal_scrollbar);
        state.vertical_scrollbar = nullptr;
        state.horizontal_scrollbar = nullptr;
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
        state->widget = create_host(*self, *state);
        linux::openmotif::accordion_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void accordion::show() const {
        auto *state = linux::openmotif::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: accordion is not created.");
        XtManageChild(state->widget);
        if (XtIsRealized(state->widget)) {
            XRaiseWindow(linux::openmotif::cached_display,
                         XtWindow(state->widget));
        }
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

} // namespace native
