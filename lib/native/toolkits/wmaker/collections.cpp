//
// Implements WINGs collection and source-editor frame routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    struct scroll_model
    {
        std::uint64_t total = 1;
        std::uint64_t page = 1;
        std::uint64_t start = 0;
        std::uint64_t line = 1;
        bool shown = false;
    };

    linux::wmaker::native_collection *state_for(native::wnd &owner) {
        if (auto *accordion = dynamic_cast<native::accordion *>(&owner))
            return linux::wmaker::accordion_bindings.object_from_handle(
                accordion);
        if (auto *icons = dynamic_cast<native::icon_view *>(&owner))
            return linux::wmaker::icon_view_bindings.object_from_handle(
                icons);
        if (auto *tree = dynamic_cast<native::tree_view *>(&owner))
            return linux::wmaker::tree_view_bindings.object_from_handle(
                tree);
        if (auto *table = dynamic_cast<native::table_view *>(&owner))
            return linux::wmaker::table_view_bindings
                .object_from_handle(table);
        if (auto *editor = dynamic_cast<native::code_edit *>(&owner))
            return linux::wmaker::code_edit_bindings
                .object_from_handle(editor);
        return nullptr;
    }

    std::uint64_t sum_column_widths(const native::table_view &table) {
        std::uint64_t result = 0;
        for (const auto &column : table.get_columns()) {
            if (column.visible)
                result += column.width;
        }
        return result;
    }

    int table_header_height(native::table_view &table) {
        if (!table.get_header_visible())
            return 0;
        auto appearance = native::theme::create(table.get_gpx());
        return std::max(1, appearance->defaults().header_height);
    }

    int table_row_height(native::table_view &table) {
        if (table.get_row_height())
            return std::max(1, static_cast<int>(*table.get_row_height()));
        auto appearance = native::theme::create(table.get_gpx());
        return std::max(1, appearance->defaults().table_row_height);
    }

    struct table_scroll_models
    {
        scroll_model vertical;
        scroll_model horizontal;
        int header = 0;
        int body_width = 0;
        int body_height = 0;
    };

    table_scroll_models scroll_models(native::table_view &table) {
        table_scroll_models result;
        constexpr int extent = SCROLLER_WIDTH;
        const native::size dimensions = table.get_dimensions();
        result.header = table_header_height(table);
        result.body_width = dimensions.w;
        result.body_height = std::max(
            0, static_cast<int>(dimensions.h) - result.header);
        const int row = table_row_height(table);
        const std::uint64_t total_rows =
            table.get_display_row_count();
        const bool needs_vertical =
            total_rows * static_cast<std::uint64_t>(row) >
            static_cast<std::uint64_t>(result.body_height);
        result.vertical.shown =
            table.get_vertical_scrollbar_policy() ==
                native::scrollbar_policy::always ||
            (table.get_vertical_scrollbar_policy() ==
                 native::scrollbar_policy::automatic &&
             needs_vertical);
        if (result.vertical.shown)
            result.body_width = std::max(0,
                                         result.body_width - extent);

        const std::uint64_t content_width = sum_column_widths(table);
        const bool needs_horizontal =
            content_width >
            static_cast<std::uint64_t>(result.body_width);
        result.horizontal.shown =
            table.get_horizontal_scrollbar_policy() ==
                native::scrollbar_policy::always ||
            (table.get_horizontal_scrollbar_policy() ==
                 native::scrollbar_policy::automatic &&
             needs_horizontal);
        if (result.horizontal.shown)
            result.body_height = std::max(0,
                                          result.body_height - extent);

        result.vertical.total = std::max<std::uint64_t>(1, total_rows);
        result.vertical.page = std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(result.body_height / row));
        result.vertical.start = table.get_vertical_scroll_row();
        result.vertical.line = 1;

        result.horizontal.total = std::max<std::uint64_t>(
            1, content_width);
        result.horizontal.page = std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(result.body_width));
        result.horizontal.start = static_cast<std::uint64_t>(
            std::max(0, table.get_horizontal_scroll_offset()));
        result.horizontal.line = 20;
        return result;
    }

    scroll_model vertical_model(native::wnd &owner) {
        scroll_model result;
        const native::size dimensions = owner.get_dimensions();
        result.page = std::max<std::uint64_t>(1, dimensions.h);
        result.line = 20;
        if (auto *icons = dynamic_cast<native::icon_view *>(&owner)) {
            result.total = std::max<std::uint64_t>(
                1, icons->get_content_dimensions().h);
            result.start = static_cast<std::uint64_t>(
                std::max(0, icons->get_scroll_offset()));
        } else if (auto *tree =
                       dynamic_cast<native::tree_view *>(&owner)) {
            const int row = tree->get_visible_item_count() > 0
                                ? std::max<int>(
                                      1, tree->get_row_bounds(0).d.h)
                                : 20;
            const std::uint64_t count = tree->get_visible_item_count();
            result.total = std::max<std::uint64_t>(
                1, count * static_cast<std::uint64_t>(row));
            result.start = static_cast<std::uint64_t>(
                std::max(0, tree->get_scroll_offset()));
            result.line = static_cast<std::uint64_t>(row);
        }
        result.shown = result.total > result.page;
        return result;
    }

    void set_parameters(WMScroller *scroller,
                        const scroll_model &model) {
        if (!scroller)
            return;
        const std::uint64_t total = std::max<std::uint64_t>(1,
                                                            model.total);
        const std::uint64_t page = std::min(total,
                                            std::max<std::uint64_t>(
                                                1, model.page));
        const std::uint64_t maximum = total - page;
        const std::uint64_t start = std::min(model.start, maximum);
        const float knob = static_cast<float>(
            static_cast<double>(page) / static_cast<double>(total));
        const float value = maximum == 0
                                ? 0.0F
                                : static_cast<float>(
                                      static_cast<double>(start) /
                                      static_cast<double>(maximum));
        WMSetScrollerParameters(scroller, value, knob);
    }

    void set_visible(WMScroller *scroller,
                     bool &was_visible,
                     bool visible) {
        if (!scroller || was_visible == visible)
            return;
        if (visible) {
            WMMapWidget(scroller);
            WMRaiseWidget(scroller);
        } else {
            WMUnmapWidget(scroller);
        }
        was_visible = visible;
    }

    void synchronize_scrollers(native::wnd &owner) {
        auto *state = state_for(owner);
        if (!state || !state->vertical_scroller)
            return;
        constexpr int extent = SCROLLER_WIDTH;
        if (auto *table = dynamic_cast<native::table_view *>(&owner)) {
            const table_scroll_models models = scroll_models(*table);
            const bool vertical = models.vertical.shown &&
                                  models.body_height > 0;
            if (vertical) {
                WMMoveWidget(state->vertical_scroller,
                             models.body_width,
                             0);
                WMResizeWidget(state->vertical_scroller,
                               extent,
                               models.header + models.body_height);
                set_parameters(state->vertical_scroller,
                               models.vertical);
            }
            set_visible(state->vertical_scroller,
                        state->vertical_visible,
                        vertical);

            const bool horizontal = models.horizontal.shown &&
                                    models.body_width > 0;
            if (horizontal && state->horizontal_scroller) {
                WMMoveWidget(state->horizontal_scroller,
                             0,
                             models.header + models.body_height);
                WMResizeWidget(state->horizontal_scroller,
                               models.body_width,
                               extent);
                set_parameters(state->horizontal_scroller,
                               models.horizontal);
            }
            set_visible(state->horizontal_scroller,
                        state->horizontal_visible,
                        horizontal);
            return;
        }

        const scroll_model model = vertical_model(owner);
        const bool visible = model.shown && owner.get_dimensions().h > 0;
        if (visible) {
            WMMoveWidget(state->vertical_scroller,
                         std::max(0,
                                  static_cast<int>(
                                      owner.get_dimensions().w) - extent),
                         0);
            WMResizeWidget(state->vertical_scroller,
                           extent,
                           owner.get_dimensions().h);
            set_parameters(state->vertical_scroller, model);
        }
        set_visible(state->vertical_scroller,
                    state->vertical_visible,
                    visible);
    }

    std::uint64_t scrolled_start(WMScroller *scroller,
                                 const scroll_model &model) {
        const std::uint64_t page = std::min(
            model.total, std::max<std::uint64_t>(1, model.page));
        const std::uint64_t maximum = model.total > page
                                          ? model.total - page
                                          : 0;
        std::uint64_t result = std::min(model.start, maximum);
        const WMScrollerPart part = WMGetScrollerHitPart(scroller);
        const std::uint64_t page_step = std::max<std::uint64_t>(
            1, page > model.line ? page - model.line : page);
        switch (part) {
        case WSDecrementPage:
            result -= std::min(result, page_step);
            break;
        case WSIncrementPage:
            result = std::min(maximum, result + page_step);
            break;
        case WSDecrementLine:
            result -= std::min(result, model.line);
            break;
        case WSIncrementLine:
            result = std::min(maximum, result + model.line);
            break;
        case WSDecrementWheel: {
            const std::uint64_t amount = model.line * 3;
            result -= std::min(result, amount);
            break;
        }
        case WSIncrementWheel:
            result = std::min(maximum, result + model.line * 3);
            break;
        case WSKnob:
        case WSKnobSlot:
            result = static_cast<std::uint64_t>(std::llround(
                static_cast<double>(WMGetScrollerValue(scroller)) *
                static_cast<double>(maximum)));
            break;
        case WSNoPart:
            break;
        }
        return std::min(result, maximum);
    }

    void scroller_action(WMWidget *widget, void *data) {
        auto *owner = static_cast<native::wnd *>(data);
        if (!owner || !owner->get_created())
            return;
        auto *state = state_for(*owner);
        auto *scroller = reinterpret_cast<WMScroller *>(widget);
        if (!state || !scroller)
            return;

        if (auto *table = dynamic_cast<native::table_view *>(owner)) {
            const table_scroll_models models = scroll_models(*table);
            if (scroller == state->vertical_scroller) {
                table->on_native_scroll(
                    static_cast<std::size_t>(
                        scrolled_start(scroller, models.vertical)),
                    table->get_horizontal_scroll_offset());
            } else if (scroller == state->horizontal_scroller) {
                table->on_native_scroll(
                    table->get_vertical_scroll_row(),
                    static_cast<int>(
                        scrolled_start(scroller, models.horizontal)));
            }
        } else {
            const scroll_model model = vertical_model(*owner);
            const std::uint64_t start = scrolled_start(scroller, model);
            const std::int64_t delta =
                static_cast<std::int64_t>(start) -
                static_cast<std::int64_t>(model.start);
            const int bounded = static_cast<int>(std::clamp<std::int64_t>(
                delta,
                std::numeric_limits<int>::min(),
                std::numeric_limits<int>::max()));
            if (auto *icons = dynamic_cast<native::icon_view *>(owner))
                icons->on_native_scroll(bounded);
            else if (auto *tree =
                         dynamic_cast<native::tree_view *>(owner))
                tree->on_native_scroll(bounded);
        }
        synchronize_scrollers(*owner);
    }

    void resize_backbuffer(native::wnd &owner, int width, int height) {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(&owner);
        if (!cache || width <= 0 || height <= 0)
            return;
        if (cache->backbuffer != None && cache->width == width &&
            cache->height == height)
            return;
        if (cache->backbuffer != None)
            XFreePixmap(linux::wmaker::display, cache->backbuffer);
        const Window target = linux::wmaker::drawable(&owner);
        if (target == None)
            return;
        cache->backbuffer = XCreatePixmap(
            linux::wmaker::display,
            target,
            width,
            height,
            DefaultDepth(linux::wmaker::display,
                         DefaultScreen(linux::wmaker::display)));
        cache->width = width;
        cache->height = height;
    }

    void paint(native::wnd &owner) {
        if (!owner.get_created())
            return;
        synchronize_scrollers(owner);
        const native::size dimensions = owner.get_dimensions();
        auto &graphics = owner.get_gpx();
        resize_backbuffer(owner, dimensions.w, dimensions.h);
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(&owner);
        const Window target = linux::wmaker::drawable(&owner);
        if (!cache || cache->backbuffer == None || !cache->gc ||
            target == None)
            return;
        native::rect invalid(0, 0, dimensions.w, dimensions.h);
        graphics.set_clip(invalid);
        native::wnd_paint_event event(invalid, graphics);
        owner.on_native_paint(event);
        XSetClipMask(linux::wmaker::display, cache->gc, None);
        XCopyArea(linux::wmaker::display,
                  cache->backbuffer,
                  target,
                  cache->gc,
                  0,
                  0,
                  cache->width,
                  cache->height,
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

    void handle_event(XEvent *event, void *data) {
        auto *owner = static_cast<native::wnd *>(data);
        if (!owner || !event || !owner->get_created())
            return;
        switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0)
                paint(*owner);
            break;
        case ConfigureNotify:
            resize_backbuffer(*owner,
                              event->xconfigure.width,
                              event->xconfigure.height);
            owner->on_native_resize(native::size(
                event->xconfigure.width, event->xconfigure.height));
            synchronize_scrollers(*owner);
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
                XSetInputFocus(event->xbutton.display,
                               event->xbutton.window,
                               RevertToParent,
                               event->xbutton.time);
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                event->type == ButtonPress
                    ? native::mouse_action::press
                    : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            if (event->type == ButtonRelease) {
                if (auto *table =
                        dynamic_cast<native::table_view *>(owner)) {
                    auto *state = state_for(*owner);
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
                    auto *state = state_for(*owner);
                    const native::tree_view_hit hit = tree->hit_test(
                        native::point(event->xbutton.x,
                                      event->xbutton.y));
                    const native::tree_item_id item = hit.id;
                    if (state &&
                        hit.part == native::tree_view_hit_part::row &&
                        item != native::invalid_tree_item_id &&
                        item == state->last_tree_item &&
                        event->xbutton.time - state->last_click <= 400) {
                        tree->on_native_double_click(item);
                        state->last_click = 0;
                        state->last_tree_item =
                            native::invalid_tree_item_id;
                    } else if (state) {
                        state->last_click = event->xbutton.time;
                        state->last_tree_item =
                            hit.part == native::tree_view_hit_part::row
                                ? item
                                : native::invalid_tree_item_id;
                    }
                    break;
                }
                auto *icons = dynamic_cast<native::icon_view *>(owner);
                auto *state = state_for(*owner);
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

    linux::wmaker::native_collection *create_frame(
        native::wnd &owner) {
        WMFrame *frame = WMCreateFrame(
            linux::wmaker::parent_widget(&owner));
        if (!frame)
            throw std::runtime_error(
                "Window Maker/WINGs: failed to create collection frame.");
        const native::point position =
            linux::wmaker::control_position(&owner);
        const native::size dimensions = owner.get_dimensions();
        WMMoveWidget(frame, position.x, position.y);
        WMResizeWidget(frame, dimensions.w, dimensions.h);
        // A table paints an inset relief around its data viewport while its
        // WINGs scrollers retain their own native frames. Framing this common
        // parent would incorrectly combine those independent controls.
        WMSetFrameRelief(
            frame,
            dynamic_cast<native::table_view *>(&owner)
                ? WRFlat
                : WRSunken);
        linux::wmaker::wnd_bindings.register_pair(frame, &owner);
        WMCreateEventHandler(
            WMWidgetView(frame),
            ExposureMask | StructureNotifyMask | FocusChangeMask |
                KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                PointerMotionMask,
            handle_event,
            &owner);
        auto *state = new linux::wmaker::native_collection();
        state->frame = frame;
        if (dynamic_cast<native::icon_view *>(&owner) ||
            dynamic_cast<native::tree_view *>(&owner) ||
            dynamic_cast<native::table_view *>(&owner)) {
            state->vertical_scroller = WMCreateScroller(frame);
            if (!state->vertical_scroller) {
                WMDestroyWidget(frame);
                delete state;
                throw std::runtime_error(
                    "Window Maker/WINGs: failed to create native "
                    "vertical scroller.");
            }
            WMSetScrollerArrowsPosition(state->vertical_scroller,
                                        WSAMaxEnd);
            WMSetScrollerAction(state->vertical_scroller,
                                scroller_action,
                                &owner);
        }
        if (dynamic_cast<native::table_view *>(&owner)) {
            state->horizontal_scroller = WMCreateScroller(frame);
            if (!state->horizontal_scroller) {
                WMDestroyWidget(frame);
                delete state;
                throw std::runtime_error(
                    "Window Maker/WINGs: failed to create native "
                    "horizontal scroller.");
            }
            WMSetScrollerArrowsPosition(state->horizontal_scroller,
                                        WSAMaxEnd);
            WMSetScrollerAction(state->horizontal_scroller,
                                scroller_action,
                                &owner);
        }
        return state;
    }

    void destroy_frame(native::wnd &owner,
                       linux::wmaker::native_collection *state) {
        owner.on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(&owner);
        if (state && state->frame)
            WMDestroyWidget(state->frame);
        delete state;
    }
} // namespace

namespace linux::wmaker
{
    native_collection *create_collection_frame(native::wnd &owner) {
        return create_frame(owner);
    }

    void destroy_collection_frame(native::wnd &owner,
                                  native_collection *state) {
        destroy_frame(owner, state);
    }
} // namespace linux::wmaker

namespace native
{
    void accordion::apply_items() { invalidate(); }
    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = create_frame(*self);
        linux::wmaker::accordion_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }
    void accordion::show() const {
        auto *state = linux::wmaker::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: accordion is not created.");
        // Accordions can also be created dynamically after their parent
        // was realized (the docking compass uses this path).  WINGs does
        // not realize such descendants merely by mapping them.
        WMRealizeWidget(state->frame);
        WMMapWidget(state->frame);
        WMRaiseWidget(state->frame);
    }
    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = linux::wmaker::accordion_bindings
                          .object_from_handle(self);
        destroy_frame(*self, state);
        linux::wmaker::accordion_bindings.unregister_by_handle(self);
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
        auto *state = create_frame(*self);
        linux::wmaker::icon_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }
    void icon_view::show() const {
        auto *state = linux::wmaker::icon_view_bindings
                          .object_from_handle(
                              const_cast<icon_view *>(this));
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: icon_view is not created.");
        WMRealizeWidget(state->frame);
        WMMapWidget(state->frame);
    }
    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *state = linux::wmaker::icon_view_bindings
                          .object_from_handle(self);
        destroy_frame(*self, state);
        linux::wmaker::icon_view_bindings.unregister_by_handle(self);
    }

} // namespace native
