# Advanced Table Views

GEMix uses the shared virtual-table geometry and scrollbar drag calculations.
Its root peer captures a thumb drag through release; painting uses GEM-style
arrow buttons, a flat outlined thumb, and a stippled track. The hard AES visible
clip also applies to table rows and borders, preventing owner content from
overpainting a modeless window above it.

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
  Native painting still owns headers, cells and selection. All data modes
  add only the requested grid edges in item post-paint, using native
  row/header rectangles and system shadow color. This keeps materialized and
  virtual grids equally visible: ListView omits native grids in group view,
  uses faint themed lines otherwise, and cannot select just one grid axis.
  `LVS_EX_GRIDLINES` stays disabled to avoid a second, inconsistent grid pass.
- macOS uses `NSTableView`, reusable cell views, native columns, stripes, grid
  styles, selection, and sort descriptors. Stock cells use native text/image
  subviews rather than repainting them through the portable theme. Native
  group rows supply a spanning label and a disclosure button; stock headers
  retain `NSTableHeaderCell` painting. Derived controls retain the explicit
  drawing extension. Both data modes use AppKit's full-width table style and
  independently selectable native horizontal and vertical grid axes.
  Grid colors blend the system content background toward the label color
  by 24%; alternating row backgrounds use 9%. Dynamic colors retain contrast
  in light and dark appearances. The backend assigns `gridColor` and native
  `NSTableRowView.backgroundColor`; it does not override row or grid painting.
  Clip-view notifications update the portable first-row
  cache without feeding rounded row positions back into native scrolling.
  Explicit scrolling uses native row rectangles, including at the end of a
  million-row virtual model. Removing an explicit row height restores the
  native default. Group labels preserve the model title without appending a
  second count, and cell/viewport clipping prevents content crossing borders.
- OpenMotif uses one buffered Motif-themed viewport for both data modes,
  retaining real `XmScrollBar` peers. Headers, column widths, grouping,
  selection, grid lines and row colors therefore follow the same path.
  This replaces the incompatible materialized `XmContainer` detail path,
  which could not supply virtual rows or grid lines. Only visible rows are
  requested, and invalidation does not clear the window before presentation.
- Haiku links its Open Tracker-licensed `BColumnListView` implementation for
  materialized data and uses a `BControlLook` viewport host for virtual data.
  Materialized group fields paint clipped portions of one full-width row;
  native latches own disclosure and their gutter continues horizontal grid
  lines. Last-column fitting subtracts the outline margin, dividers, and native
  scrollbar from the viewport, including after resize. Heading alignment is
  independent of right-aligned cell values. Both modes use the compact native
  title-strip height, native header background, and real `BScrollBar`
  controls. Virtual scrollbars map logical row positions without constructing
  native rows and preserve both range endpoints. Both modes honor the same
  row-background hook, including alternating-row changes in the latch gutter.
  Their nominal row pitch is identical: the native inclusive `BRow` height is
  one less than the portable pixel count. Haiku disables page compression so
  the virtual table retains that pitch even with a partially visible last row.
  Scroll limits count fully visible rows, allowing the final model row to be
  revealed completely. Paging, painting, and native scrollbar placement use
  the same body geometry, including both scrollbar reservations.
- Athena, XView/OLGX, WINGs, SDL2, and GEM use backend-owned hosts and theme
  resources when their toolkit has no complete table widget. Athena attaches
  genuine Xaw `Scrollbar` children in the reserved strips. Its native stippled
  thumb, button-1/button-3 relative scrolling and button-2 dragging map to
  portable row and pixel offsets; both endpoints remain reachable without
  materializing virtual rows. Its scrollbar border also separates the table
  body from the track, while the header spans the full table width. The XView host
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
last visible page across the complete viewport when the theme's
`table_fit_visible_rows` metric is enabled (the default). Haiku instead clips
the final row to preserve native row spacing. The fitted final header occupies the top-right area
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
