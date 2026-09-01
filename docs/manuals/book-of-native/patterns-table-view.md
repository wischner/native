# Advanced Table Views

`table_view` is the advanced, model-backed sibling of the lightweight `list`
control. It presents semantic columns, image-and-text cells, stable row
selection, headings, sort requests, groups, search, and native scrolling
without making the portable model depend on a platform widget.

## Model and identity

A view borrows a `table_model`. The model returns a logical row count, a
stable non-zero ID for each row, and a cell only when asked. `table_store`
provides owned materialized rows for ordinary data sets. An application model
may instead generate values, query a database, or page remote results.

Native item indexes are transient adapter details. Selection, activation,
scroll anchors, and refresh recovery use stable IDs. Group expansion belongs
to each view rather than mutating the shared model.

The visible-row mapper stores group ranges and expansion state, not one entry
per logical row. This is why a million-row model remains a million-row model
rather than becoming a million-widget allocation.

## Native adaptation

- Windows uses `WC_LISTVIEW` report mode. Virtual mode adds `LVS_OWNERDATA`;
  explicit materialized mode uses native ListView groups.
- macOS uses `NSTableView`, reusable cell views, native columns, stripes, grid
  styles, selection, and sort descriptors.
- OpenMotif uses `XmContainer` in `XmDETAIL` layout for explicit materialized
  mode and the Motif-themed compact host for virtual mode.
- Haiku links its Open Tracker-licensed `BColumnListView` implementation for
  materialized data and uses a `BControlLook` viewport host for virtual data.
- Athena, XView/OLGX, WINGs, SDL2, and GEM use backend-owned hosts and theme
  resources when their toolkit has no complete table widget.

Each adapter applies cached state during creation. Programmatic changes do not
emit action signals; translated user actions update the portable cache and
emit once.

## Search and invalidation

The default model search supports exact, prefix, and substring matching,
case-sensitive or deterministic Unicode case-folded comparison, selected
columns, a start row, and optional wrap. Indexed models can override the same
function. `find_and_reveal()` selects a result, expands its group if allowed,
and scrolls it into view.

Model notifications identify resets, inserted, removed, changed, or regrouped
row ranges. The view rebuilds compact mapping and native presentation while
preserving still-valid selection and viewport anchors by stable ID.

See the application-facing
[Advanced Table Views](../programming-native/14-advanced-table-views.md)
chapter for complete examples.
