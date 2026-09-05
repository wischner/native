//
// Composes stock Xaw Scrollbar widgets around painted tables, collections
// and canvases. Xaw owns stippled thumbs, pointer grabs and button actions;
// this adapter maps its fractions/pixel deltas to portable scroll state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "scrollbars.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Scrollbar.h>

#include "globals.h"
#include "../../table_render.h"

namespace
{
    using namespace native;
    using linux::x11::xaw_scroll_axis;

    std::uint64_t maximum(const xaw_scroll_axis &axis) {
        return axis.total > axis.page ? axis.total - axis.page : 0;
    }

    void scroll(xaw_scroll_axis &axis, std::uint64_t position) {
        position = std::min(position, maximum(axis));
        auto &owner = *axis.owner;
        if (auto *table = dynamic_cast<table_view *>(&owner)) {
            table->on_native_scroll(axis.horizontal ? table->get_vertical_scroll_row() : position,
                axis.horizontal ? static_cast<int>(position) : table->get_horizontal_scroll_offset());
        } else if (auto *icons = dynamic_cast<icon_view *>(&owner)) {
            icons->on_native_scroll(static_cast<int>(position) - icons->get_scroll_offset());
        } else if (auto *tree = dynamic_cast<tree_view *>(&owner)) {
            tree->on_native_scroll(static_cast<int>(position) - tree->get_scroll_offset());
        } else if (auto *surface = dynamic_cast<canvas *>(&owner)) {
            auto current = surface->get_scroll_position();
            const auto value = static_cast<std::int32_t>(
                axis.origin + static_cast<std::int64_t>(position));
            if (axis.horizontal) current.x = value;
            else current.y = value;
            surface->on_native_scroll(current);
        }
        // Reflect repeated native events immediately, before the next paint.
        axis.position = position;
    }

    void jump(Widget, XtPointer data, XtPointer value) {
        auto &axis = *static_cast<xaw_scroll_axis *>(data);
        const float fraction = *static_cast<float *>(value);
        if (!std::isfinite(fraction)) return;
        const long double position = std::clamp(fraction, 0.0f, 1.0f) *
            static_cast<long double>(axis.total);
        scroll(axis, static_cast<std::uint64_t>(std::min<long double>(
            maximum(axis), std::round(position))));
    }

    void move(xaw_scroll_axis &axis, long double delta) {
        const long double position = std::clamp<long double>(
            axis.position + delta, 0, maximum(axis));
        scroll(axis, static_cast<std::uint64_t>(position));
    }

    void step(Widget, XtPointer data, XtPointer value) {
        auto &axis = *static_cast<xaw_scroll_axis *>(data);
        const auto pixels = reinterpret_cast<std::intptr_t>(value);
        if (!pixels) return;
        const long double amount = std::max<long double>(1,
            std::round(std::abs(static_cast<long double>(pixels)) *
                axis.page / std::max(1, axis.length)));
        move(axis, pixels < 0 ? -amount : amount);
    }

    void wheel(Widget, XtPointer data, XEvent *event, Boolean *dispatch) {
        if (event->type != ButtonPress ||
            (event->xbutton.button != Button4 && event->xbutton.button != Button5)) return;
        auto &axis = *static_cast<xaw_scroll_axis *>(data);
        const int amount = dynamic_cast<table_view *>(axis.owner) && !axis.horizontal ? 3 : 48;
        move(axis, event->xbutton.button == Button4 ? -amount : amount);
        *dispatch = False;
    }

    void place(xaw_scroll_axis &axis, Widget host, rect track,
        std::uint64_t total, std::uint64_t page, std::uint64_t position,
        std::int64_t origin = 0) {
        axis.total = total;
        axis.page = page;
        axis.position = std::min(position, maximum(axis));
        axis.origin = origin;
        if (track.d.w < 3 || track.d.h < 3) {
            if (axis.widget && XtIsManaged(axis.widget)) XtUnmanageChild(axis.widget);
            return;
        }
        if (!axis.widget) {
            axis.widget = XtVaCreateWidget(axis.horizontal ? "horizontalScrollbar" : "verticalScrollbar",
                scrollbarWidgetClass, host, XtNorientation,
                axis.horizontal ? XtorientHorizontal : XtorientVertical,
                XtNborderWidth, 1, XtNborderColor, BlackPixelOfScreen(XtScreen(host)), nullptr);
            XtAddCallback(axis.widget, XtNjumpProc, jump, &axis);
            XtAddCallback(axis.widget, XtNscrollProc, step, &axis);
            XtAddEventHandler(axis.widget, ButtonPressMask, False, wheel, &axis);
        }
        axis.length = std::max(1, int(axis.horizontal ? track.d.w : track.d.h) - 2);
        XtVaSetValues(axis.widget,
            XtNhorizDistance, track.x1(), XtNvertDistance, track.y1(),
            XtNwidth, track.d.w - 2, XtNheight, track.d.h - 2, nullptr);
        const float shown = total ? std::min(1.0L, static_cast<long double>(page) / total) : 1;
        const float top = total ? static_cast<long double>(axis.position) / total : 0;
        XawScrollbarSetThumb(axis.widget, top, shown);
        XtSetSensitive(axis.widget, maximum(axis) != 0);
        if (!XtIsManaged(axis.widget)) XtManageChild(axis.widget);
    }
}

namespace linux::x11
{
    bool has_native_scrollbars(const native::wnd &owner) {
        return dynamic_cast<const table_view *>(&owner) ||
            dynamic_cast<const icon_view *>(&owner) ||
            dynamic_cast<const tree_view *>(&owner) ||
            dynamic_cast<const canvas *>(&owner);
    }

    void synchronize_scrollbars(native::wnd &owner, Widget host) {
        if (!has_native_scrollbars(owner)) return;
        auto *state = native::detail::peer_state<xaw_scrollbars>(owner);
        if (!state) {
            state = new xaw_scrollbars;
            state->horizontal.owner = state->vertical.owner = &owner;
            state->horizontal.horizontal = true;
            native::detail::assign_peer_state(owner, state);
        }
        const auto metrics = theme::create(owner.get_gpx())->defaults();
        const int extent = std::max(3, metrics.scrollbar_extent);
        const auto dimensions = owner.get_dimensions();
        const int width = dimensions.w, height = dimensions.h;
        rect horizontal, vertical;
        if (auto *table = dynamic_cast<table_view *>(&owner)) {
            const rect body = native::detail::table_body_bounds(*table, metrics);
            const int row = std::max<int>(1, table->get_row_height().value_or(metrics.table_row_height));
            const int rounding = metrics.table_fit_visible_rows ? row - 1 : 0;
            const auto page = std::max(1, (int(body.d.h) + rounding) / row);
            std::uint64_t columns = 0;
            for (const auto &column : table->get_columns()) if (column.visible) columns += column.width;
            if (body.x2() < width) vertical = rect(body.x2(), body.y1(), width - body.x2(), body.d.h);
            if (body.y2() < height) horizontal = rect(0, body.y2(), body.d.w, height - body.y2());
            place(state->vertical, host, vertical, table->get_display_row_count(), page, table->get_vertical_scroll_row());
            place(state->horizontal, host, horizontal, columns, body.d.w, table->get_horizontal_scroll_offset());
        } else if (auto *surface = dynamic_cast<canvas *>(&owner)) {
            const bool v = surface->get_vertical_scrollbar_visible();
            const bool h = surface->get_horizontal_scrollbar_visible();
            const auto viewport = surface->get_client_bounds();
            const auto content = surface->get_content_bounds();
            const auto position = surface->get_scroll_position();
            if (v) vertical = rect(std::max(0, width - extent), 0, std::min(width, extent), std::max(0, height - (h ? extent : 0)));
            if (h) horizontal = rect(0, std::max(0, height - extent), std::max(0, width - (v ? extent : 0)), std::min(height, extent));
            place(state->vertical, host, vertical, content.height, viewport.d.h,
                std::max<std::int64_t>(0, std::int64_t(position.y) - content.y), content.y);
            place(state->horizontal, host, horizontal, content.width, viewport.d.w,
                std::max<std::int64_t>(0, std::int64_t(position.x) - content.x), content.x);
        } else {
            std::uint64_t total = 0;
            int position = 0;
            if (auto *icons = dynamic_cast<icon_view *>(&owner)) {
                total = icons->get_content_dimensions().h;
                position = icons->get_scroll_offset();
            } else if (auto *tree = dynamic_cast<tree_view *>(&owner)) {
                const int row = tree->get_visible_item_count() ? tree->get_row_bounds(0).d.h : metrics.list_item_height;
                total = std::min<std::uint64_t>(std::numeric_limits<int>::max(),
                    std::uint64_t(tree->get_visible_item_count()) * row);
                position = tree->get_scroll_offset();
            }
            if (total > dimensions.h)
                vertical = rect(std::max(0, width - extent), 0, std::min(width, extent), height);
            place(state->vertical, host, vertical, total, height, position);
        }
    }
}
