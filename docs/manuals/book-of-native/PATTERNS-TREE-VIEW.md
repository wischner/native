# Patterns: Classic Tree Views

This chapter expands Architecture Section 15. `tree_view` is an owned,
single-selection hierarchy, separate from `list`, `icon_view` and the
model-backed virtual `table_view`. Application construction and input examples
are in [Programming Native, Chapter 13](../programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md).

## Values, identity and visibility

Each `tree_view_item` owns its child vector, text, enabled/expanded flags and
an optional shared immutable image. Non-zero `tree_item_id` values identify
items independently of native handles or visible row numbers. The core checks
uniqueness across the entire hierarchy before publishing a replacement.
`add_item()` edits a candidate copy and delegates to `set_items()`; a rejected
candidate does not publish duplicate IDs or an unknown parent.

The implementation in `lib/native/tree_view.cpp` recursively constructs visible
`{id, depth}` rows. Descendants appear only when their ancestors are expanded.
This is a classic owned hierarchy, not the table's virtual data model. Visible
row queries currently rebuild that flattened vector rather than maintaining a
second persistent model with native row identity.

Replacing items retains selection when its ID still exists. Removing a branch
removes its descendants and clears selection if the selected ID no longer
exists. Collapsing an ancestor of the selection moves selection to the branch.
Collapsing with no selection preserves the empty sentinel. Replacement reveals
a preserved selected ID by silently expanding its ancestors, so it cannot leave
keyboard selection on a hidden row. Boundary regressions are recorded in
[ADJUST.md](../../ADJUST.md), A12.

Motif retains an `XmContainer` outline inside an automatic
`XmScrolledWindow`. A separate native `XmFrame` owns the outer sunken border;
the scrolled window has no shadow of its own. Motif clears its entire scrolling
surface when expand/collapse changes the outline's preferred dimensions, even
when the viewport dimensions stay unchanged. Keeping the frame outside that
surface prevents those internal clears from erasing the border. Explicit border
changes update the frame's shadow resource. Item gadgets survive branch changes.

Library-requested repaints target the inner container with translated
coordinates. Native disclosure actions still perform their own row relayout and
scrollbar updates; this routing alone does not suppress Motif's internal clears.
`native_motif_runtime_tests --tree-repaint` isolates the native disclosure,
frame-expose/pixel, border-property and scrolling regression for CDE runs.

## State and notifications

Programmatic setters validate and cache items, selection, expansion, icon size,
connector visibility, border visibility, presentation and scroll offset. When
created, they call the corresponding backend apply hooks and invalidate.
Programmatic changes do not emit user-action signals.

A backend calls `on_native_selection()`, `on_native_expansion()`,
`on_native_navigation()`, `on_native_double_click()`, `on_native_activate()` or
`on_native_scroll()` with portable IDs and commands. The core updates the
cache before emitting the matching selection, expansion or activation signal.
Disabled rows stay visible but cannot be selected, disclosed or activated by
user input. Native callback code must treat signal dispatch as a possible
lifetime boundary; the shared callback-lifetime review is A01 in ADJUST.md.

The core implements visible-row navigation, enabled endpoints, paging,
parent/child movement, expansion and activation. Disclosure hit testing is
separate from row selection. Connector lines default off; callers can enable
them explicitly. The border defaults on. A backend without a distinct
three-dimensional presentation keeps its normal native appearance.

## Native and painted adapters

| Backend | Implementation |
| --- | --- |
| Windows | `WC_TREEVIEW`, with private mappings preserving full 64-bit public IDs |
| macOS | `NSOutlineView` in `NSScrollView`, native reusable text/image cells |
| Haiku | `BOutlineListView` and native scrolling with custom icon-bearing items |
| OpenMotif | `XmContainer` outline with `XmIconGadget` entries for both presentations |
| X11/Athena, SDL2, OPEN LOOK, Window Maker, GEMix | Shared portable tree drawing adapted to their child hosts and input dispatch |

Native adapters store toolkit item/row mappings behind the peer or native
callback-facing bindings. Painted adapters share row, disclosure, icon and
scrollbar geometry with the renderer and input helpers. They must not turn
stable IDs into truncated native indexes or keep a second public item model.
The maintained [control audit](../../notes/CUSTOM.md) records the native,
hybrid and custom classifications and the justification for painted trees.

## Metrics, scrolling and tests

The shared core caches row metrics and clamps scroll offset against visible
content. Reapplying unchanged metrics preserves manual scrolling; navigation
and actual row-size changes can reveal selection. Painted tree rows use the
current theme's selection text, disclosure and scrollbar primitives. Native
backends adapt their own viewport and item geometry.

`native_window_api_tests` exercises hierarchy ownership, unique IDs, flattening,
selection, removal, disclosure geometry, disabled items, navigation and silent
programmatic changes. Runtime collection/control suites verify native resource
lifecycle and backend interaction to their documented extent. The review's
source and runtime evidence, missing edge cases and platform-specific follow-up
are in [ADJUST.md](../../ADJUST.md) and the [feature matrix](FEATURE-MATRIX.md).


Haiku model replacement guards native row painting and latch geometry while
rebuilding BOutlineListView. Removing a selected native item synchronously
paints and queries its latch before the old row is released, even though the
portable model already contains the replacement. Those transitional calls
skip portable IDs/geometry until rebuilding completes. The settings runtime
fixture exercises clear, re-add and resource recreation, including native
selected-row teardown.


GTK rows display optional portable images through native GtkImage children
backed by owned GdkMemoryTexture pixels. The tree's logical icon size controls
rendering, while the texture retains the source image resolution. The native
label follows the image with spacing and ellipsizes within the row. Replacing
items rebuilds native children and releases their old texture references.
The settings fixture checks the displayed texture's pixels, not just the
portable image descriptor.

GTK folder disclosure uses a compact native GtkExpander without a label.
The row owns its image and caption separately, so folder and leaf rows have
the same natural height. Native expansion notifications update the portable
model; rebuilding rows keeps the existing synchronization guard.
