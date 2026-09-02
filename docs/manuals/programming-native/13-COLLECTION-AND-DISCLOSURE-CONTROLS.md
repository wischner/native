# Chapter 13: Collection and Disclosure Controls

`accordion`, `icon_view`, and `tree_view` build library panes, asset browsers,
thumbnail pickers, and classic hierarchies without stretching the text-only
`list` control beyond its purpose.

Use `list` for a compact vertical sequence of strings. Use `icon_view` when
each item has an image, a label, spatial arrow-key navigation, activation,
wrapping, or scrolling. Use `tree_view` when parent/child structure and
independent branch expansion are part of the data.

## Classic tree hierarchies

Tree items recursively own their children. Every item has a unique non-zero
stable ID, optional shared image, expansion state, and enabled state:

```cpp
auto folder = std::make_shared<native::img>(16, 16);
native::tree_view files({
    {"Project", folder, 100,
     {{"include", folder, 110,
       {{"native.h", nullptr, 111},
        {"tree_view.h", nullptr, 112}}, true},
      {"src", folder, 120,
       {{"main.cpp", nullptr, 121}}, false}}, true},
    {"README.md", nullptr, 200}
}, native::rect(0, 0, 320, 360));

files.set_presentation(native::tree_view_presentation::native);
```

Use `invalid_tree_item_id` for no selection. Programmatic selection and
expansion are silent; user actions emit stable IDs:

```cpp
files.on_selection_change.connect([](native::tree_item_id id) {
    inspect_item(id);
    return true;
});
files.on_expanded_change.connect(
    [](native::tree_item_id id, bool expanded) {
        remember_expansion(id, expanded);
        return true;
    });
files.on_item_activate.connect([](native::tree_item_id id) {
    open_item(id);
    return true;
});
```

Left collapses a branch or moves to its parent. Right expands a branch or
moves to its first child. Up, Down, Home, End, Page Up, and Page Down navigate
visible enabled rows; Space toggles and Enter activates. A row double click
uses the platform's normal branch behavior and activates the item.

`reveal_item()` expands ancestors and scrolls the item into view.
`set_lines_visible()` controls classic connector lines where the native peer
offers that choice. The native default is theme-specific: Window Maker uses
indentation and transparent right/down disclosure triangles without connector
branches. An explicit `set_lines_visible()` choice survives later native-theme
metric synchronization. Replacing items retains selection by stable ID when
the item survives.

`set_presentation()` selects the normal platform presentation or the optional
`three_dimensional` outline. On CDE, `native` follows the InfoLib Book List:
the live CDE data colors, flat full-width selection, compact right/down
triangles, uniform row geometry, and vertically centered icons and text. The
three-dimensional choice retains the Motif `XmContainer` outline for software
that wants that later Motif style.

On CDE, overflowing lists, icon views, trees, and tables expose native Motif
scrollbars. Their values remain synchronized with the portable scroll state,
so wheel, keyboard, and scrollbar navigation reveal the same items.

## Icon items and image ownership

An icon item carries a UTF-8 label, a shared immutable image, a stable
application ID, and an enabled flag:

```cpp
auto image = std::make_shared<native::img>(48, 48);
image->get_gpx()
    .clear(native::rgba(0, 0, 0, 0))
    .set_ink(native::rgba(35, 110, 210, 255))
    .draw_ellipse(native::rect(4, 4, 40, 40), true);

native::icon_view icons({
    {"Circle", image, 1001, true},
    {"Disabled sample", image, 1002, false}
}, native::rect(0, 0, 360, 240));
icons.set_icon_size({48, 48})
     .set_label_mode(native::icon_view_label_mode::below);
```

The `shared_ptr<const img>` stored by each item keeps the pixels alive. The
same immutable image may be shared by several items or controls. Replacing or
clearing items releases the control's references.

`below`, `beside`, and `hidden` label modes affect presentation but not the
selection model. Images retain their aspect ratio, fit without implicit
upscaling, and preserve alpha.

## Selection and activation

`icon_view` is single-selection in this release. `-1` means no selection.
Programmatic setters update the cache and native widget without emitting a
signal. User changes emit `on_selection_change(int)` exactly once; double
click or Enter emits `on_item_activate(int)`.

```cpp
icons.on_selection_change.connect([](int index) {
    select_asset(index);
    return true;
});
icons.on_item_activate.connect([](int index) {
    insert_asset(index);
    return true;
});
```

Arrow keys navigate the wrapping grid. Home and End select its endpoints;
Page Up and Page Down move by a visible page. Scrolling remains encapsulated
by the control, while `set_scroll_offset()` is available for restoring UI
state.

## Accordion ownership and modes

An accordion owns its section model but borrows each content `wnd`. The
application must keep those objects alive longer than the accordion section.
Add content before creating the accordion; the accordion assigns the parent
and creates the currently expanded body when its own backend resource exists.

Declare borrowed controls before the accordion in a containing class so C++
reverse destruction detaches them safely:

```cpp
class library_pane final
{
public:
    library_pane()
        : shapes({}, 0, 0, 320, 240),
          colors({}, 0, 0, 320, 240),
          sections(12, 12, 340, 360) {
        sections.add_item("Shapes", shapes);
        sections.add_item("Colors", colors);
    }

    native::icon_view shapes;
    native::icon_view colors;
    native::accordion sections;
};
```

`accordion_mode::single` is the default and gives the expanded body all space
remaining below the visible headers. `accordion_mode::multiple` lets bodies
toggle independently and retains each content control's preferred height.
`set_expanded_index()` selects one section (or `-1`) without emitting;
user toggles emit `on_expanded_change(int)`.

When a header has focus, Up/Down and Home/End move among headers, while Enter
or Space toggles the focused section. Focus inside a content control keeps
that control's normal keyboard handling.

## Native mappings and accessibility

Windows icon views use `WC_LISTVIEW`; macOS uses `NSCollectionView`, and the
macOS accordion uses an `NSStackView` with disclosure buttons. Trees map to
`WC_TREEVIEW`, `NSOutlineView`, Motif `XmContainer`, and Haiku
`BOutlineListView`; toolkits without an outline widget compose their native
focus/input host and semantic theme parts. The portable API hides those
differences.

Native widget mappings expose their normal collection and disclosure
accessibility. Custom mappings remain keyboard-focusable, expose visible
focus, and retain selected, disabled, and expanded state. A native widget may
vary spacing, font metrics, selection shape, and scrollbar details without
changing the model contract.

On OPEN LOOK, the canvas-backed icon and tree hosts attach genuine XView
`Scrollbar` objects. Their elevator and cable update the portable scroll
offset, while programmatic scrolling updates the native view position. The
portable renderer reserves the XView scrollbar extent but does not paint a
second, emulated track beneath it.

OPEN LOOK disclosure marks are centered using the actual OLGX glyph metrics,
not an assumed triangle size. Tree connector lines terminate before the mark
and continue below it only for an expanded branch, keeping normal and selected
row glyphs unobstructed.

On Window Maker, collection content uses the desktop panel gray rather than
an editor-white surface. Accordion and group headers share the raised dark-gray
header role used by native Window Maker tables and tabs. Icon and tree hosts
attach genuine WINGs vertical scrollers; the native child occupies the extent
reserved by the semantic renderer, and the portable scrollbar painter is
suppressed rather than drawn underneath it.
WINGs stock disclosure arrows are composited as transparent glyphs over the
row or accordion header. Their embedded pixmap paper is not copied, so normal,
selected, collapsed, and expanded indicators retain the surrounding surface.

The Vision application's **Collection controls** window demonstrates a
Libraries accordion with enough alpha-bearing thumbnails to force scrolling
and a classic expandable project tree on every backend.

Previous: [Building, linking, and distributing](12-BUILDING-AND-DISTRIBUTING.md).

Next: [Advanced table views](14-ADVANCED-TABLE-VIEWS.md).
