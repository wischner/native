//
// Declares the portable editable and selection-only combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "theme.h"
#include "wnd.h"

namespace native
{
    namespace detail { class control_render_access; }

    enum class combo_box_style
    {
        drop_down_list,
        editable
    };

    class combo_box : public wnd
    {
    public:
        combo_box(std::vector<std::string> items = {},
                  combo_box_style style = combo_box_style::drop_down_list,
                  coord x = 0,
                  coord y = 0,
                  dim width = 160,
                  dim height = 24);
        combo_box(const std::vector<std::string> &items,
                  combo_box_style style,
                  const rect &bounds);
        ~combo_box() override;

        const std::vector<std::string> &get_items() const;
        combo_box &set_items(std::vector<std::string> items);
        combo_box &add_item(const std::string &item);
        combo_box &remove_item(std::size_t index);
        combo_box &clear_items();

        int get_selected_index() const;
        combo_box &set_selected_index(int index);

        const std::string &get_text() const;
        combo_box &set_text(const std::string &text);

        combo_box_style get_style() const;
        combo_box &set_style(combo_box_style style);

        virtual void on_native_selection(int index);
        virtual void on_native_text(const std::string &text);
        virtual void on_native_drop_down(bool open);

        void create() const override;
        void destroy() const override;
        void show() const override;

        signal<int> on_selection_change;
        signal<std::string> on_text_change;
        signal<bool> on_drop_down;

    protected:
        virtual void draw_control(gpx &graphics,
                                  theme &appearance,
                                  const rect &bounds,
                                  const theme::state &state);
        virtual void apply_items();
        virtual void apply_selected_index();
        virtual void apply_text();
        virtual void apply_style();

    private:
        friend class detail::control_render_access;

        std::vector<std::string> _items;
        combo_box_style _style;
        int _selected_index = -1;
        std::string _text;

        void validate_index(int index) const;
    };
} // namespace native
