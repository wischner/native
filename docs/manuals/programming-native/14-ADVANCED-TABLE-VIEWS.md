# Chapter 14: Advanced Table Views

Use `list` for a small, owned sequence of text strings. Use `table_view` when
rows need multiple columns, headings, images, groups, sorting, multiple
selection, search, or virtual data.

Columns can be appended with either `add_column()` or builder syntax:

```cpp
table << native::table_column{1, "Name", 180}
      << native::table_column{2, "Value", 120};
```

The operator is only an append shorthand; configuration remains in named
setters.

## Define columns and a materialized model

Columns and cells are associated by stable semantic IDs. Rows also have
stable, non-zero IDs so selection survives reordering and model changes.

```cpp
native::table_column name;
name.id = 1;
name.title = "Name";
name.width = 220;
name.sortable = true;

native::table_column size;
size.id = 2;
size.title = "Size";
size.width = 90;
size.alignment = native::table_alignment::end;

native::table_store rows({
    {101, {{1, {"Logo", logo_image.get()}},
           {2, {"24 KB", nullptr}}}},
    {102, {{1, {"Hero", hero_image.get()}},
           {2, {"81 KB", nullptr}}}}
});

native::table_view table(native::rect(12, 12, 480, 280));
table.set_columns({name, size})
     .set_model(&rows)
     .set_alternating_rows(true)
     .set_grid_lines(native::table_grid_lines::horizontal);
```

The final visible column fills spare viewport width by default on every
backend. Disable that presentation rule only when trailing blank space is
intentional:

```cpp
table.set_fill_last_column(false);
```

Fitting does not rewrite the configured column width; overflowing tables keep
their configured widths and scroll horizontally.

Rows use the active backend's native table height by default. Set a complete
row height when an application needs denser or roomier rows, and clear it to
return to the native default:

```cpp
table.set_row_height(28);          // Text, image, and selection are 28 px high.
table.set_row_height(std::nullopt); // Restore the backend-native table height.
```

This is the full row and selection rectangle, not extra text padding. Window
Maker defaults to the taller row spacing used by its reference Task Manager.
Its table also receives a final one-pixel inset viewport relief after headers
are laid out, so edge-aligned header cells cannot erase the black top/left and
white bottom/right edges. Native scrollbar reservations are excluded from
that relief because the WINGs scrollers carry their own frames.

Library-painted adapters also draw their outer frame after the row pass, use a
gentle theme-provided gray for alternating rows, and distribute the last
visible page through the viewport so every row is complete without leaving a
fixed empty strip. This fitting is controlled by
`theme::metrics::table_fit_visible_rows`. Haiku turns it off to keep
materialized and virtual row heights identical; a final row may be partially
visible, just as in its native table. Both Haiku modes use native scrollbars.
Their final fitted header covers the top-right area above a vertical scrollbar.
Scrollbars include arrow step buttons and a gripped thumb with matching raised
relief, and the table frame is drawn after all of them so the right and bottom
edges remain visible. Platforms with native table widgets retain their system
scrollbar.

`table_view` borrows its model, and each `table_cell` borrows its optional
image. Keep both alive longer than the view. `table_store` owns the row and
string values and is convenient for small and medium data sets.

Alternating rows use colors supplied by the active native theme. OPEN LOOK
uses the XView control CMS light-grey background and highlight-white pair;
selection retains the toolkit's inverse black highlight. Its native vertical
scrollbar occupies the right-hand reserved strip below the table header.
Window Maker tables use the reference desktop's light-gray list body and
alternating row, dark inactive-selection highlight, and raised dark-gray
header role instead of editor white. They attach genuine WINGs vertical and
horizontal scrollers. When all semantic columns fit, the trailing visible
column consumes unused viewport width without changing its configured width;
horizontal overflow retains the configured widths.
Collapsible Window Maker group rows use compact filled right/down disclosure
triangles, matching native task-list groups without importing the opaque
resource-paper square carried by the WINGs arrow pixmaps.

On macOS, materialized and virtual tables both use `NSTableView` with native
headers, cells, selection and scrollbars. Grid lines and alternating row
backgrounds use contrasting blends of the system content and label colors,
which resolve for light or dark appearance. AppKit paints both; there is no
library-painted grid overlay. `set_grid_lines()` selects either axis, both,
or neither, and `set_alternating_rows(false)` restores plain row backgrounds.

## Groups, selection, and sorting

Groups describe ordered, disjoint ranges in logical model-row coordinates:

```cpp
rows.set_groups({
    {10, "Documents", 0, 40, true, true},
    {20, "Images", 40, 60, true, true}
});

table.set_selection_mode(native::table_selection_mode::multiple);
table.on_selection_change.connect(
    [](const std::vector<native::table_row_id> &selected) {
        update_actions(selected);
        return true;
    });
table.on_sort_request.connect([&table](native::table_sort sort) {
    reorder_model(sort);
    return true;
});
```

The view displays the requested sort indicator but does not reorder the model.
The application performs sorting and notifies the view through the model.
Programmatic setters do not emit user-action signals.

Collapsed groups hide display rows without deleting logical rows or their
stable selection. `find_and_reveal()` expands a collapsible group when needed.

## Search and reveal

Search can be exact, prefix, or substring based and can target selected
columns:

```cpp
native::table_search query;
query.text = "logo";
query.match = native::table_search_match::substring;
query.case_mode = native::table_search_case::insensitive;
query.columns = {1};

if (table.find_and_reveal(query))
    use(table.get_selected_rows().front());
```

The default model search is lazy and Unicode aware. A database or indexed
model can override `find()` to avoid a linear scan while keeping the same
query contract.

## Virtual models

Derive from `table_model` when values can be generated or loaded on demand:

```cpp
class generated_rows final : public native::table_model
{
public:
    std::size_t row_count() const override { return 1000000; }

    native::table_row_id row_id(std::size_t row) const override {
        return row + 1;
    }

    native::table_cell cell(
        std::size_t row,
        native::table_column_id column) const override {
        if (column == 1)
            return {"Row " + std::to_string(row + 1), nullptr};
        return {};
    }
};
```

Install it with `table_data_mode::virtualized`. The view keeps only compact
group and viewport state; it does not copy a million row objects. Windows maps
this to owner-data report ListView, macOS asks its `NSTableView` data source on
demand, and custom toolkit hosts paint only mapped viewport rows.

The Vision **Advanced tables** window demonstrates four columns, cell icons,
120 rows, two collapsible groups, row/grid/selection toggles, substring search,
and a separate million-row virtual table with a deep scroll action.

Previous: [Collection and disclosure controls](13-COLLECTION-AND-DISCLOSURE-CONTROLS.md).
