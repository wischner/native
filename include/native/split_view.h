//
// Declares a portable two-pane split view with borrowed child windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "theme.h"
#include "wnd.h"

namespace native
{
    // Selects how a split view arranges its two panes.
    enum class split_orientation
    {
        // The first pane is left of the second pane.
        horizontal,
        // The first pane is above the second pane.
        vertical
    };

    // Presents two borrowed child windows separated by a draggable splitter.
    class split_view : public wnd
    {
    public:
        split_view(wnd &first,
                   wnd &second,
                   split_orientation orientation =
                       split_orientation::horizontal,
                   coord x = 0,
                   coord y = 0,
                   dim width = 320,
                   dim height = 240);

        split_view(wnd &first,
                   wnd &second,
                   split_orientation orientation,
                   const rect &bounds);

        ~split_view() override;

        wnd &get_first() const;
        wnd &get_second() const;

        split_orientation get_orientation() const;
        split_view &set_orientation(split_orientation orientation);

        // Return the first pane's share of available space, from 0 to 1.
        float get_ratio() const;

        // Set the first pane's share without emitting a user-change event.
        split_view &set_ratio(float ratio);

        dim get_first_minimum() const;
        dim get_second_minimum() const;
        split_view &set_minimums(dim first, dim second);

        dim get_splitter_size() const;
        split_view &set_splitter_size(dim size);

        rect get_first_bounds() const;
        rect get_second_bounds() const;
        rect get_splitter_bounds() const;

        // Accept a ratio produced by a native splitter drag.
        virtual void on_native_ratio(float ratio);

        void create() const override;
        void destroy() const override;
        void show() const override;

        // Emits after the user moves the splitter; programmatic changes are silent.
        signal<float> on_ratio_change;

    protected:
        void on_bounds_changed() override;
        virtual void apply_orientation();
        virtual void apply_ratio();
        virtual void apply_minimums();
        virtual void apply_splitter_size();

    private:
        wnd *_first;
        wnd *_second;
        split_orientation _orientation;
        float _ratio = 0.5f;
        dim _first_minimum = 40;
        dim _second_minimum = 40;
        dim _splitter_size = 6;
        bool _content_hosts_are_panes = false;
        bool _dragging = false;

        void refresh_contents();
        int resolved_first_extent() const;
        float ratio_from_position(const point &position) const;
        void draw(gpx &graphics);
        bool handle_click(const mouse_event &event);
        bool handle_move(const point &position);
    };
} // namespace native
