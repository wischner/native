# Split views and tabs

`split_view` places two borrowed child windows on opposite sides of a native
separator. Construct it with both uncreated panes, choose an orientation, and
then parent the split view like any other control:

```cpp
native::list project({"include", "lib", "tests"});
native::text_edit editor("", native::text_edit_mode::multi_line);
native::split_view split(
    project,
    editor,
    native::split_orientation::horizontal,
    native::rect(20, 20, 720, 440));

split.set_ratio(0.3f)
     .set_minimums(140, 220);
split.set_parent(&window);
```

`horizontal` places panes left and right; `vertical` places them top and
bottom. `set_ratio()` is programmatic and signal-silent. A user drag updates
the cached ratio and emits `on_ratio_change`. The split view automatically
uses the corresponding horizontal- or vertical-resize cursor over its
divider; applications do not need to set it themselves. Portable dividers use
the same borderless panel color as their host and retain only a compact center
grip, so the gutter reads as part of the window rather than a framed control.

The implementation uses Xaw `Paned`, `BSplitView`, `XmPanedWindow`,
`WMSplitView`, and `NSSplitView` on their respective backends. Other systems
use their normal native child-host mechanism and the portable divider
interaction. Window Maker observes its native pane sizes after a divider drag
and refits both borrowed children to those exact sizes, so neither pane retains
its old width or clips its trailing border.

`tab_view` borrows one child window per item and creates only the selected
page. Add pages with `add_item()` or append-only `operator<<`, select with
`set_selected_index()`, and listen to `on_selection_change` for
user-originated changes. Native tab
backends include `BTabView`, `XmNotebook`, `WMTabView`, `NSTabView`, and the
Win32 tab common control.

Tabs occupy the top edge by default. Top, bottom, left, and right placement
is portable and may be set
before or after native creation:

```cpp
native::tab_view tabs(20, 20, 420, 260);
tabs.set_tab_placement(native::tab_placement::bottom)
    .set_page_frame_visible(false);
tabs << native::tab_page("General", general_page)
     << native::tab_page("Advanced", advanced_page);
```

Left and right tabs rotate their labels: left labels read bottom-to-top and
right labels read top-to-bottom.

`get_tab_bounds()` reports labels at the selected edge and
`get_content_bounds()` reports the remaining page rectangle: below top tabs
or above bottom tabs, to the right of left tabs, or to the left of right
tabs. Changing placement preserves every item, the selected
index, and the selected borrowed page. Like `set_selected_index()`, the
placement setter is programmatic and does not emit `on_selection_change`.
Every placement has a directional free edge rather than merely translating
a top-facing control. Native controls are used where their APIs support the
requested orientation and label direction; other combinations use Native's
matching directional renderer.

The complete page frame is visible by default for compatibility.
`set_page_frame_visible(false)` switches to strip-only presentation: the
page is flush to the control's cross-axis edges and a single separator spans
the complete boundary between the page and the tab labels. The getter is
`get_page_frame_visible()`. The setter works before or after `create()`,
preserves items, selection, and borrowed page contents, and does not emit a
selection-change signal. Painted tabs overlap the adjoining page or separator
by one device pixel, so every placement meets its page without a visible gap.
Window Maker retains one page-edge line at selected bottom/right joins, which
matches selected top/left tabs.
Window Maker inactive bottom and right tabs suppress the redundant closure
beside the page edge and shadow the free edge, leaving one light line without
changing the selected-tab overlap.

Both controls detach borrowed children during destruction. The application
must keep page and pane objects alive longer than their containing control.
