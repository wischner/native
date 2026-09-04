//
// Hosts real Haiku scrollbars around a virtual table without creating
// native row objects. Large ranges retain exact first/last positions.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "table_scrollbars.h"

#include <algorithm>
#include <cmath>
#include <ScrollBar.h>
#include <native.h>

#include "globals.h"
#include "../../table_render.h"

namespace
{
    class table_scrollbar final : public BScrollBar
    {
    public:
        table_scrollbar(native::table_view &owner, orientation axis)
            : BScrollBar(BRect(0, 0, 14, 14),
                axis == B_VERTICAL ? "native_table_vertical" : "native_table_horizontal",
                nullptr, 0, 0, axis)
            , _owner(owner)
            , _axis(axis) {
            Hide();
        }

        void synchronize(std::size_t maximum, std::size_t page,
                         std::size_t position) {
            _suppress = true;
            _maximum = maximum;
            _range = static_cast<float>(std::min<std::size_t>(maximum, 1000000));
            SetRange(0, _range);
            const long double total = static_cast<long double>(maximum) + page;
            SetProportion(total > 0 ? static_cast<float>(page / total) : 1);
            const float unit = maximum > 0 ? _range / maximum : 1;
            SetSteps(unit, std::max(unit, unit * page));
            SetValue(maximum > 0 ? static_cast<float>(
                static_cast<long double>(std::min(position, maximum)) /
                maximum * _range) : 0);
            _suppress = false;
        }

        void ValueChanged(float value) override {
            if (_suppress || !_owner.get_created()) return;
            const std::size_t position = _range <= 0 ? 0
                : value >= _range ? _maximum
                : static_cast<std::size_t>(std::round(
                    static_cast<long double>(std::max(0.0f, value)) /
                    _range * _maximum));
            _owner.on_native_scroll(
                _axis == B_VERTICAL ? position : _owner.get_vertical_scroll_row(),
                _axis == B_HORIZONTAL ? static_cast<int>(position)
                                      : _owner.get_horizontal_scroll_offset());
        }

    private:
        native::table_view &_owner;
        orientation _axis;
        std::size_t _maximum = 0;
        float _range = 0;
        bool _suppress = true;
    };

    void place(BScrollBar *bar, bool visible, BRect frame) {
        if (visible) {
            bar->MoveTo(frame.LeftTop());
            bar->ResizeTo(std::max(0.0f, frame.Width()),
                          std::max(0.0f, frame.Height()));
            if (bar->IsHidden(bar)) bar->Show();
        } else if (!bar->IsHidden(bar)) bar->Hide();
    }
}

namespace haiku
{
    void create_table_scrollbars(native::table_view &table) {
        auto *state = table_view_bindings.object_from_handle(&table);
        state->vertical_scrollbar = new table_scrollbar(table, B_VERTICAL);
        state->horizontal_scrollbar = new table_scrollbar(table, B_HORIZONTAL);
        state->view->AddChild(state->vertical_scrollbar);
        state->view->AddChild(state->horizontal_scrollbar);
        refresh_table_scrollbars(table);
    }

    void refresh_table_scrollbars(native::table_view &table) {
        auto *state = table_view_bindings.object_from_handle(&table);
        if (!state || !state->vertical_scrollbar) return;
        const auto metrics = native::theme::create(table.get_gpx())->defaults();
        const native::rect body = native::detail::table_body_bounds(table, metrics);
        const int header = body.p.y;
        const int row = std::max<int>(1,
            table.get_row_height().value_or(metrics.table_row_height));
        const int width = table.get_dimensions().w;
        const int height = table.get_dimensions().h;
        int columns = 0;
        for (const auto &column : table.get_columns())
            if (column.visible) columns += column.width;
        const auto count = table.get_display_row_count();
        const bool vertical = body.x2() < width;
        const bool horizontal = body.y2() < height;
        const int body_width = body.d.w;
        const int body_height = body.d.h;
        const std::size_t page = std::max(1, body_height / row);
        place(state->vertical_scrollbar, vertical,
            BRect(body_width, header, width - 1, header + body_height - 1));
        place(state->horizontal_scrollbar, horizontal,
            BRect(0, header + body_height, body_width - 1, height - 1));
        static_cast<table_scrollbar *>(state->vertical_scrollbar)->synchronize(
            count > page ? count - page : 0, page, table.get_vertical_scroll_row());
        static_cast<table_scrollbar *>(state->horizontal_scrollbar)->synchronize(
            std::max(0, columns - body_width), body_width, table.get_horizontal_scroll_offset());
    }
}
