# Advanced Table Views

`table_view` is the advanced, model-backed sibling of the lightweight `list`
control. It presents semantic columns, image-and-text cells, stable row
selection, headings, sort requests, groups, search, and native scrolling
without making the portable model depend on a platform widget.

Columns may be appended with the named `add_column()` operation or with the
shared collection-builder spelling `table << native::table_column{...}`.
The operator performs exactly one append and is never used for table
configuration.

Scrollbar visibility follows `scrollbar_policy`, which is declared in
`include/native/scrollbar.h` rather than in `table_view.h`. It is one shared
control concept: `canvas` uses the same type with the same qualified name, and
`automatic`, `always`, and `never` mean the same thing on both.

The final visible column consumes unused viewport width by default across all
backends. `set_fill_last_column(false)` preserves only configured widths. The
fitted width is presentation state, so the semantic `table_column::width`
remains unchanged and becomes authoritative again when horizontal scrolling is
needed.

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
  resources when their toolkit has no complete table widget. The XView host
  attaches genuine east-side and bottom `Scrollbar` objects to the exact
  extents reserved by the semantic renderer and obtains alternating-row
  grey/white colors from its control CMS. The WINGs host likewise attaches
  real vertical and horizontal scrollers, uses the Window Maker session's
  gray list/header/selection roles, and stretches the trailing visible column
  over unused viewport width without changing semantic column geometry. Group
  disclosure uses the native compact filled right/down triangle so the WINGs
  stock pixmap's resource paper cannot erase or box the indicator.

Each adapter applies cached state during creation. Programmatic changes do not
emit action signals; translated user actions update the portable cache and
emit once.

Library-painted table input uses the row and header metrics synchronized when
the backend host is created. SDL child controls paint into the root renderer,
so pointer routing computes the hit from that cache and never asks an
unbound child for its own graphics context.

Those hosts paint the complete outer border after headers, rows, and portable
scrollbars, use a subtle theme-supplied alternate row color, and distribute the
last visible page across the complete viewport so no row is clipped and no
empty bottom strip remains. The fitted final header occupies the top-right area
above a vertical scrollbar. Portable step arrows and the gripped thumb share
the same raised relief. Native-widget adapters keep their system scrollbars
while applying the same final-column fitting policy.

## Search and invalidation

The default model search supports exact, prefix, and substring matching,
case-sensitive or deterministic Unicode case-folded comparison, selected
columns, a start row, and optional wrap. Indexed models can override the same
function. `find_and_reveal()` selects a result, expands its group if allowed,
and scrolls it into view.

Model notifications identify resets, inserted, removed, changed, or regrouped
row ranges. The view rebuilds compact mapping and native presentation.
Incremental changes preserve still-valid selection and viewport anchors by
stable ID; a complete reset drops obsolete anchors, removes stale selection,
and clamps its scroll position before querying the replacement model.

See the application-facing
[Advanced Table Views](../programming-native/14-ADVANCED-TABLE-VIEWS.md)
chapter for complete examples.
