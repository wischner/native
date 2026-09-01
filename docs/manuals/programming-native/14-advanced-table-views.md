# Chapter 14: Advanced Table Views

Use `list` for a small, owned sequence of text strings. Use `table_view` when
rows need multiple columns, headings, images, groups, sorting, multiple
selection, search, or virtual data.

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

`table_view` borrows its model, and each `table_cell` borrows its optional
image. Keep both alive longer than the view. `table_store` owns the row and
string values and is convenient for small and medium data sets.

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

Previous: [Collection and disclosure controls](13-collection-and-disclosure-controls.md).
