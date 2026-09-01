//
// Implements absolute, grid, and nested layout algorithms.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <native/layout.h>
#include <native/wnd.h>

namespace
{
    template <typename value_type>
    void remove_ptr(std::vector<value_type *> &items,
                    value_type *item) {
        items.erase(std::remove(items.begin(), items.end(), item),
                    items.end());
    }

    template <typename value_type>
    void push_unique(std::vector<value_type *> &items,
                     value_type *item) {
        if (!item)
            return;
        if (std::find(items.begin(), items.end(), item) == items.end())
            items.push_back(item);
    }

    native::coord clamp_coord(int v) {
        const int lo =
            static_cast<int>(std::numeric_limits<native::coord>::min());
        const int hi =
            static_cast<int>(std::numeric_limits<native::coord>::max());
        return static_cast<native::coord>(
            std::max(lo, std::min(hi, v)));
    }

    native::dim clamp_dim(int v) {
        if (v <= 0)
            return 0;
        const int hi =
            static_cast<int>(std::numeric_limits<native::dim>::max());
        return static_cast<native::dim>(std::min(v, hi));
    }

    // Add weighted tracks until a requested index range exists.
    void ensure_track_count(std::vector<native::grid_length> &tracks,
                            int required) {
        while (static_cast<int>(tracks.size()) < required)
            tracks.push_back(native::grid_length::star());
    }

    // Resolve fixed and weighted track definitions to integer pixels.
    std::vector<int>
    compute_track_sizes(const std::vector<native::grid_length> &defs,
                        int total) {
        const int available = std::max(0, total);
        const int n = static_cast<int>(defs.size());

        if (n <= 0)
            return {available};

        std::vector<int> sizes(static_cast<std::size_t>(n), 0);

        float star_sum = 0.0f;
        for (const auto &d : defs) {
            const float v = std::max(0.0f, d.value);
            if (d.type == native::grid_length::unit::star)
                star_sum += v;
        }

        int fixed_used = 0;
        for (int i = 0; i < n; ++i) {
            const auto &d = defs[static_cast<std::size_t>(i)];
            if (d.type == native::grid_length::unit::pixel) {
                const int sz = static_cast<int>(
                    std::lround(std::max(0.0f, d.value)));
                sizes[static_cast<std::size_t>(i)] = sz;
                fixed_used += sz;
            }
        }

        const int remaining = std::max(0, available - fixed_used);

        if (star_sum > 0.0f) {
            int star_used = 0;
            std::vector<int> star_indices;
            for (int i = 0; i < n; ++i) {
                const auto &d = defs[static_cast<std::size_t>(i)];
                if (d.type != native::grid_length::unit::star)
                    continue;

                const float weight = std::max(0.0f, d.value);
                const int sz = static_cast<int>(
                    std::floor((remaining * weight) / star_sum));
                sizes[static_cast<std::size_t>(i)] = sz;
                star_used += sz;
                star_indices.push_back(i);
            }

            int left = remaining - star_used;
            for (int j = 0; left > 0 && !star_indices.empty();
                 ++j, --left) {
                const int index = star_indices[static_cast<std::size_t>(
                    j % static_cast<int>(star_indices.size()))];
                ++sizes[static_cast<std::size_t>(index)];
            }
        } else if (n > 0) {
            sizes.back() += remaining;
        }

        return sizes;
    }

    // Compute the starting coordinate of every resolved track.
    std::vector<int>
    compute_track_offsets(const std::vector<int> &sizes, int origin) {
        std::vector<int> offsets(sizes.size(), origin);
        int cur = origin;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            offsets[i] = cur;
            cur += std::max(0, sizes[i]);
        }
        return offsets;
    }

    // Holds the resolved pixel size and origin of every track on one
    // grid axis. Resolving depends only on the arranged bounds, so a
    // relayout pass resolves each axis once and shares it between
    // every child instead of repeating the work per cell.
    struct track_axis
    {
        std::vector<int> sizes;
        std::vector<int> offsets;
    };

    // Resolve one axis of track definitions into pixels and origins.
    track_axis
    resolve_track_axis(const std::vector<native::grid_length> &defs,
                       int origin,
                       int total) {
        track_axis axis;
        axis.sizes = compute_track_sizes(defs, total);
        axis.offsets = compute_track_offsets(axis.sizes, origin);
        return axis;
    }

    // Compute one cell rectangle from already resolved track axes.
    native::rect cell_rect(const track_axis &rows,
                           const track_axis &columns,
                           int row,
                           int column,
                           int row_span,
                           int column_span,
                           int margin) {
        const int nr = static_cast<int>(rows.sizes.size());
        const int nc = static_cast<int>(columns.sizes.size());
        if (nr <= 0 || nc <= 0)
            return native::rect(0, 0, 0, 0);

        const int r = std::max(0, std::min(row, nr - 1));
        const int c = std::max(0, std::min(column, nc - 1));
        const int rs = std::max(1, row_span);
        const int cs = std::max(1, column_span);
        const int r2 = std::min(nr, r + rs) - 1;
        const int c2 = std::min(nc, c + cs) - 1;

        int x = columns.offsets[static_cast<std::size_t>(c)];
        int y = rows.offsets[static_cast<std::size_t>(r)];

        int w = 0;
        for (int i = c; i <= c2; ++i)
            w += columns.sizes[static_cast<std::size_t>(i)];

        int h = 0;
        for (int i = r; i <= r2; ++i)
            h += rows.sizes[static_cast<std::size_t>(i)];

        const int m = std::max(0, margin);
        x += m;
        y += m;
        w = std::max(0, w - (m * 2));
        h = std::max(0, h - (m * 2));

        return native::rect(
            clamp_coord(x), clamp_coord(y), clamp_dim(w), clamp_dim(h));
    }
} // namespace

namespace native
{
    layout_manager::~layout_manager() = default;

    absolute_layout_manager::absolute_layout_manager() = default;

    grid_length grid_length::pixels(float px) {
        grid_length l;
        l.value = std::max(0.0f, px);
        l.type = unit::pixel;
        return l;
    }

    grid_length grid_length::star(float weight) {
        grid_length l;
        l.value = std::max(0.0f, weight);
        l.type = unit::star;
        return l;
    }

    grid_length pixels(float value) {
        return grid_length::pixels(value);
    }

    grid_length star(float weight) {
        return grid_length::star(weight);
    }

    grid_child_layout_def::grid_child_layout_def(
        std::unique_ptr<grid_layout_manager> child_layout,
        int r,
        int c,
        int rs,
        int cs,
        int m)
        : layout(std::move(child_layout))
        , row(r)
        , column(c)
        , row_span(rs)
        , column_span(cs)
        , margin(m) {}

    grid_child_layout_def::grid_child_layout_def(
        grid_child_layout_def &&) noexcept = default;

    grid_child_layout_def &grid_child_layout_def::operator=(
        grid_child_layout_def &&) noexcept = default;

    grid_row_def row(const grid_length &length) {
        return {length};
    }

    grid_column_def column(const grid_length &length) {
        return {length};
    }

    grid_cell_def cell(wnd &child,
                       int row_idx,
                       int column_idx,
                       int row_span,
                       int column_span,
                       int margin) {
        grid_cell_def d;
        d.child = &child;
        d.row = row_idx;
        d.column = column_idx;
        d.row_span = row_span;
        d.column_span = column_span;
        d.margin = margin;
        return d;
    }

    grid_child_layout_def
    child_grid(std::unique_ptr<grid_layout_manager> layout,
               int row_idx,
               int column_idx,
               int row_span,
               int column_span,
               int margin) {
        return grid_child_layout_def(std::move(layout),
                                     row_idx,
                                     column_idx,
                                     row_span,
                                     column_span,
                                     margin);
    }

    absolute_layout_manager &
    absolute_layout_manager::operator<<(wnd &child) {
        add(child);
        return *this;
    }

    absolute_layout_manager &absolute_layout_manager::add(wnd &child) {
        add_child(&child);
        return *this;
    }

    void absolute_layout_manager::relayout(wnd *, const rect &) {
        // Absolute layout intentionally does nothing.
    }

    void absolute_layout_manager::add_child(wnd *child) {
        push_unique(_children, child);
    }

    void absolute_layout_manager::remove_child(wnd *child) {
        remove_ptr(_children, child);
    }

    const std::vector<wnd *> &
    absolute_layout_manager::children() const {
        return _children;
    }

    // A default grid starts with no tracks at all. Appending a row
    // or column is then the only thing that creates one, so a caller
    // who writes three rows gets three rows and not a leading star
    // track that quietly eats their space.
    grid_layout_manager::grid_layout_manager() = default;

    grid_layout_manager::grid_layout_manager(int rows, int columns) {
        const int rr = std::max(1, rows);
        const int cc = std::max(1, columns);
        _rows.assign(static_cast<std::size_t>(rr), grid_length::star());
        _columns.assign(static_cast<std::size_t>(cc),
                        grid_length::star());
    }

    grid_layout_manager &
    grid_layout_manager::add_row(grid_length length) {
        _rows.push_back(length);
        return *this;
    }

    grid_layout_manager &
    grid_layout_manager::add_column(grid_length length) {
        _columns.push_back(length);
        return *this;
    }

    grid_layout_manager &grid_layout_manager::add(wnd &child,
                                                  int row_idx,
                                                  int column_idx,
                                                  int row_span,
                                                  int column_span,
                                                  int margin) {
        const int r = std::max(0, row_idx);
        const int c = std::max(0, column_idx);
        const int rs = std::max(1, row_span);
        const int cs = std::max(1, column_span);

        ensure_track_count(_rows, r + rs);
        ensure_track_count(_columns, c + cs);

        push_unique(_children, &child);

        auto it = std::find_if(_placed_children.begin(),
                               _placed_children.end(),
                               [&](const placed_child &placed) {
                                   return placed.child == &child;
                               });
        if (it == _placed_children.end()) {
            _placed_children.push_back({&child, r, c, rs, cs, margin});
        } else {
            it->row = r;
            it->column = c;
            it->row_span = rs;
            it->column_span = cs;
            it->margin = margin;
        }

        return *this;
    }

    grid_layout_manager &grid_layout_manager::add_child_grid(
        std::unique_ptr<grid_layout_manager> layout,
        int row_idx,
        int column_idx,
        int row_span,
        int column_span,
        int margin) {
        if (!layout)
            return *this;

        const int r = std::max(0, row_idx);
        const int c = std::max(0, column_idx);
        const int rs = std::max(1, row_span);
        const int cs = std::max(1, column_span);

        ensure_track_count(_rows, r + rs);
        ensure_track_count(_columns, c + cs);

        _nested_grids.push_back(
            {std::move(layout), r, c, rs, cs, margin});
        return *this;
    }

    grid_layout_manager &
    grid_layout_manager::operator<<(const grid_row_def &r) {
        return add_row(r.length);
    }

    grid_layout_manager &
    grid_layout_manager::operator<<(const grid_column_def &c) {
        return add_column(c.length);
    }

    grid_layout_manager &
    grid_layout_manager::operator<<(const grid_cell_def &p) {
        if (!p.child)
            return *this;
        return add(*p.child,
                   p.row,
                   p.column,
                   p.row_span,
                   p.column_span,
                   p.margin);
    }

    grid_layout_manager &
    grid_layout_manager::operator<<(grid_child_layout_def &&nested) {
        return add_child_grid(std::move(nested.layout),
                              nested.row,
                              nested.column,
                              nested.row_span,
                              nested.column_span,
                              nested.margin);
    }

    void grid_layout_manager::relayout(wnd *parent,
                                       const rect &bounds) {
        if (!parent)
            return;
        if (_placed_children.empty() && _nested_grids.empty())
            return;

        std::vector<grid_length> row_defs = _rows;
        std::vector<grid_length> col_defs = _columns;
        if (row_defs.empty())
            row_defs.push_back(grid_length::star());
        if (col_defs.empty())
            col_defs.push_back(grid_length::star());

        // Track geometry depends only on the arranged bounds, so it
        // is resolved once for the whole pass and shared by every
        // child and nested grid below.
        const track_axis rows =
            resolve_track_axis(row_defs,
                               bounds.p.y,
                               static_cast<int>(bounds.d.h));
        const track_axis columns =
            resolve_track_axis(col_defs,
                               bounds.p.x,
                               static_cast<int>(bounds.d.w));

        for (const auto &placed : _placed_children) {
            if (!placed.child)
                continue;

            placed.child->set_bounds(cell_rect(rows,
                                               columns,
                                               placed.row,
                                               placed.column,
                                               placed.row_span,
                                               placed.column_span,
                                               placed.margin));
        }

        for (auto &nested : _nested_grids) {
            if (!nested.layout)
                continue;

            nested.layout->relayout(parent,
                                    cell_rect(rows,
                                              columns,
                                              nested.row,
                                              nested.column,
                                              nested.row_span,
                                              nested.column_span,
                                              nested.margin));
        }
    }

    void grid_layout_manager::add_child(wnd *child) {
        if (!child)
            return;

        // A window hands every one of its children to the layout it
        // installs, including the ones the caller has already placed
        // by cell. Re-placing those would silently discard the spans
        // and margins the caller asked for, so they keep the cell
        // they were given.
        if (has_placement(child))
            return;

        if (_columns.empty())
            _columns.push_back(grid_length::star());
        if (_rows.empty())
            _rows.push_back(grid_length::star());

        // Step over cells an explicit placement already covers, so an
        // auto-placed child never lands on top of one. Placements are
        // finite, so a cursor walking down the rows leaves the last
        // of them behind and this always terminates.
        while (cell_occupied(_next_auto_row, _next_auto_column))
            advance_auto_cell();

        add(*child, _next_auto_row, _next_auto_column);
        advance_auto_cell();
    }

    bool grid_layout_manager::has_placement(const wnd *child) const {
        for (const auto &placed : _placed_children) {
            if (placed.child == child)
                return true;
        }

        for (const auto &nested : _nested_grids) {
            if (nested.layout && nested.layout->has_placement(child))
                return true;
        }

        return false;
    }

    bool grid_layout_manager::cell_occupied(int row, int column) const {
        const auto covers =
            [row, column](int r, int c, int rs, int cs) {
                return row >= r && row < r + std::max(1, rs) &&
                       column >= c && column < c + std::max(1, cs);
            };

        for (const auto &placed : _placed_children) {
            if (covers(placed.row,
                       placed.column,
                       placed.row_span,
                       placed.column_span))
                return true;
        }

        for (const auto &nested : _nested_grids) {
            if (covers(nested.row,
                       nested.column,
                       nested.row_span,
                       nested.column_span))
                return true;
        }

        return false;
    }

    void grid_layout_manager::advance_auto_cell() {
        const int columns =
            std::max(1, static_cast<int>(_columns.size()));

        ++_next_auto_column;
        if (_next_auto_column < columns)
            return;

        // Only move the cursor. Placing a child is what creates the
        // row it lands in, so a cursor that has run off the end of
        // the grid does not add a row nobody occupies and nothing but
        // the existing children's share of the space would pay for.
        _next_auto_column = 0;
        ++_next_auto_row;
    }

    void grid_layout_manager::remove_child(wnd *child) {
        remove_ptr(_children, child);
        _placed_children.erase(
            std::remove_if(_placed_children.begin(),
                           _placed_children.end(),
                           [&](const placed_child &placed) {
                               return placed.child == child;
                           }),
            _placed_children.end());

        for (auto &nested : _nested_grids) {
            if (nested.layout)
                nested.layout->remove_child(child);
        }
    }

    const std::vector<wnd *> &grid_layout_manager::children() const {
        return _children;
    }
} // namespace native
