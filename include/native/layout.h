//
// Declares absolute and grid layout managers for child windows.
// Layouts own nested layouts but borrow the windows they arrange.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <vector>

#include "geometry.h"

namespace native
{
    class wnd;

    // Defines the interface used by windows to arrange child controls.
    class layout_manager
    {
    public:
        // Destroy a layout manager through its interface.
        virtual ~layout_manager();

        // Arrange registered children within parent-relative bounds.
        virtual void relayout(wnd *parent, const rect &bounds) = 0;

        // Register a non-owning child window if it is not present.
        virtual void add_child(wnd *child) = 0;

        // Remove a child window from this layout.
        virtual void remove_child(wnd *child) = 0;

        // Return the non-owning child list.
        virtual const std::vector<wnd *> &children() const = 0;
    };

    // Registers children without changing their explicit bounds.
    class absolute_layout_manager final : public layout_manager
    {
    public:
        // Construct an empty absolute layout.
        absolute_layout_manager();

        // Register a child through the stream-style interface.
        absolute_layout_manager &operator<<(wnd &child);

        // Register a child through the conventional interface.
        absolute_layout_manager &add(wnd &child);

        // Preserve every child's explicit bounds.
        void relayout(wnd *parent, const rect &bounds) override;

        // Register a non-owning child if it is not already present.
        void add_child(wnd *child) override;

        // Remove a child from this layout.
        void remove_child(wnd *child) override;

        // Return the non-owning child list.
        const std::vector<wnd *> &children() const override;

    private:
        std::vector<wnd *> _children;
    };

    // Backward-compatible name for the absolute layout manager.
    using basic_layout_manager = absolute_layout_manager;

    // Describes either a fixed-pixel or weighted grid track.
    struct grid_length
    {
        // Identifies how a row or column receives available space.
        enum class unit
        {
            pixel,
            star
        };

        float value = 1.0f;
        unit type = unit::star;

        // Create a non-negative fixed-pixel track.
        static grid_length pixels(float pixels);

        // Create a non-negative weighted track.
        static grid_length star(float weight = 1.0f);
    };

    // Create a fixed-pixel grid track for stream-style layout code.
    grid_length pixels(float value);

    // Create a weighted grid track for stream-style layout code.
    grid_length star(float weight = 1.0f);

    // Wraps a row definition for the stream-style grid builder.
    struct grid_row_def
    {
        grid_length length;
    };

    // Wraps a column definition for the stream-style grid builder.
    struct grid_column_def
    {
        grid_length length;
    };

    // Describes a child window's grid placement.
    struct grid_cell_def
    {
        wnd *child = nullptr;
        int row = 0;
        int column = 0;
        int row_span = 1;
        int column_span = 1;
        int margin = 0;
    };

    class grid_layout_manager;

    // Describes an owning nested grid's placement in its parent grid.
    struct grid_child_layout_def
    {
        std::unique_ptr<grid_layout_manager> layout;
        int row = 0;
        int column = 0;
        int row_span = 1;
        int column_span = 1;
        int margin = 0;

        // Construct a nested grid placement.
        grid_child_layout_def(
            std::unique_ptr<grid_layout_manager> child_layout,
            int row,
            int column,
            int row_span = 1,
            int column_span = 1,
            int margin = 0);

        // Move a nested grid placement.
        grid_child_layout_def(grid_child_layout_def &&) noexcept;

        // Move-assign a nested grid placement.
        grid_child_layout_def &
        operator=(grid_child_layout_def &&) noexcept;

        // Nested grid placements own their layout and cannot be copied.
        grid_child_layout_def(const grid_child_layout_def &) = delete;

        // Nested grid placements cannot be copy-assigned.
        grid_child_layout_def &
        operator=(const grid_child_layout_def &) = delete;
    };

    // Wrap a row length for the stream-style grid builder.
    grid_row_def row(const grid_length &length);

    // Wrap a column length for the stream-style grid builder.
    grid_column_def column(const grid_length &length);

    // Describe a child window's grid placement.
    grid_cell_def cell(wnd &child,
                       int row,
                       int column,
                       int row_span = 1,
                       int column_span = 1,
                       int margin = 0);

    // Describe an owning nested grid's placement.
    grid_child_layout_def
    child_grid(std::unique_ptr<grid_layout_manager> layout,
               int row,
               int column,
               int row_span = 1,
               int column_span = 1,
               int margin = 0);

    // Arranges children in fixed and proportionally weighted tracks.
    class grid_layout_manager final : public layout_manager
    {
    public:
        // Construct a grid with one weighted row and column.
        grid_layout_manager();

        // Construct a grid with positive counts of weighted tracks.
        grid_layout_manager(int rows, int columns);

        // Append a row definition.
        grid_layout_manager &add_row(grid_length length);

        // Append a column definition.
        grid_layout_manager &add_column(grid_length length);

        // Register or update a child window's grid placement.
        grid_layout_manager &add(wnd &child,
                                 int row,
                                 int column,
                                 int row_span = 1,
                                 int column_span = 1,
                                 int margin = 0);

        // Add an owning nested grid at a specified placement.
        grid_layout_manager &
        add_child_grid(std::unique_ptr<grid_layout_manager> layout,
                       int row,
                       int column,
                       int row_span = 1,
                       int column_span = 1,
                       int margin = 0);

        // Append a row through the stream-style interface.
        grid_layout_manager &operator<<(const grid_row_def &row);

        // Append a column through the stream-style interface.
        grid_layout_manager &operator<<(const grid_column_def &column);

        // Place a child through the stream-style interface.
        grid_layout_manager &operator<<(const grid_cell_def &cell);

        // Place an owning nested grid through the stream interface.
        grid_layout_manager &operator<<(grid_child_layout_def &&nested);

        // Arrange all direct and nested children within bounds.
        void relayout(wnd *parent, const rect &bounds) override;

        // Auto-place a child in the next available cell.
        void add_child(wnd *child) override;

        // Remove a child from direct and nested layouts.
        void remove_child(wnd *child) override;

        // Return the non-owning direct-child list.
        const std::vector<wnd *> &children() const override;

    private:
        // Stores normalized placement for one direct child.
        struct placed_child
        {
            wnd *child = nullptr;
            int row = 0;
            int column = 0;
            int row_span = 1;
            int column_span = 1;
            int margin = 0;
        };

        // Stores an owning nested layout and its normalized placement.
        struct nested_grid
        {
            std::unique_ptr<grid_layout_manager> layout;
            int row = 0;
            int column = 0;
            int row_span = 1;
            int column_span = 1;
            int margin = 0;
        };

        std::vector<grid_length> _rows;
        std::vector<grid_length> _columns;
        std::vector<wnd *> _children;
        std::vector<placed_child> _placed_children;
        std::vector<nested_grid> _nested_grids;
        int _next_auto_row = 0;
        int _next_auto_column = 0;
    };
} // namespace native
