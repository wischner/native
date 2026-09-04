# Patterns: Structural Panels and Paintable Canvases

This chapter expands Architecture Sections 18 and 19. Native has two concrete
child windows that carry no semantics of their own. A `panel` parents and lays
out other controls. A `canvas` hands its whole client viewport to application
painting and owns the scrollbars around it. They are deliberately separate
abstractions, and neither derives from the other.

## Why two classes and not one

`wnd` is abstract: it defines hierarchy, geometry, layout, painting, and input,
but every concrete window has to supply `create()`, `show()`, and `destroy()`.
Before these two classes, an application that simply wanted an empty region had
to pick a semantic control that happened to look empty, or subclass `wnd` and
write backend code — which is exactly what the public API exists to avoid.

The two roles do not combine cleanly:

| Responsibility | `panel` | `canvas` |
| --- | --- | --- |
| Concrete child `wnd` | Yes | Yes |
| Primary purpose | Host controls | Draw application content |
| Uses a layout manager | Yes | No |
| Parents ordinary child controls | Yes | No |
| Application owns client painting | No | Yes |
| Receives pointer input for content tools | No | Yes |
| Usable as tab or split content | Yes | Yes |
| May own inherited non-client edges | Yes | Yes |
| Stores application or model data | No | No |

A container that also painted would have to decide whether an exposed region
belongs to a child or to a paint subscriber on every repaint. A drawing surface
that also parented controls would have to reconcile a layout pass with a
scrolled content transform. Keeping them apart means each has one answer.

## The panel is inherited behavior, not new behavior

`panel` adds three constructors, a destructor, one background draw stage, and
the three protected backend hooks. Everything else already existed on `wnd`:

```cpp
native::panel page;
native::button command("Run");
native::canvas drawing;

command.set_parent(&page);
drawing.set_parent(&page);

auto grid = std::make_unique<native::grid_layout_manager>();
(*grid) << native::row(native::pixels(28))
        << native::row(native::star())
        << native::column(native::star())
        << native::cell(command, 0, 0)
        << native::cell(drawing, 1, 0);
page.set_layout(std::move(grid));
```

`set_parent()` registers the child in the panel's inherited `_children` list
and in the active layout. Installing a layout registers the children the panel
already has. Either order registers each child exactly once, and a child
assigned to a panel that already has a layout is placed immediately.

## Child lifecycle is explicit

A panel supplies a native parent. It does not create or show children on their
behalf, because it cannot know whether a registered child should start visible.
The order is:

1. Construct the panel and its children.
2. Assign the panel's parent.
3. Assign each child's parent to the panel.
4. Install the panel layout.
5. Create the panel.
6. Create and show children from `on_wnd_create`, or after panel creation.
7. Show the panel.

Step 6 works because `panel::create()` establishes the backend parent binding
and sets `get_created()` *before* emitting `on_wnd_create`. Every backend does
this in the same order for the same reason: a child resolving its native parent
during that callback has to find one.

Specialized containers — `tab_view`, `accordion`, `split_view` — keep their own
content-creation policies. They know which content is currently shown; a panel
does not.

## Backend hosts

Each backend gives the panel the toolkit's own empty container. Native's base
`draw_background()` stage requests the ordinary control-host surface before
inherited paint subscribers; a derived panel may replace just that stage.
Where the toolkit fills an empty host itself, the request resolves to the same
native resource colors:

| Backend | Host |
| --- | --- |
| X11/Athena | Xaw `Form` child, edges chained so child geometry cannot resize the panel |
| OpenMotif | `XmForm` child with `XmRESIZE_NONE` |
| OPEN LOOK/XView | Borderless `PANEL` on the frame at the accumulated child offset |
| Window Maker/WINGs | `WMFrame` with `WRFlat` relief |
| Windows | Child window of a Native class whose brush is `COLOR_BTNFACE` |
| Haiku | Child `BView` with the panel background view color and no `B_WILL_DRAW` |
| macOS | Child `NSView` filling `windowBackgroundColor` |
| SDL2, GEMix | Nested region of the emulated-control tree, filled with the themed `surface_kind::panel` |

The emulated backends are the interesting case. SDL2 and GEMix have no real
child windows, so every control is painted into one window surface and matched
against it during dispatch. Introducing a container meant those backends had to
stop matching on a control's *immediate* parent and start matching on its
*root* window, accumulating ancestor offsets into the position they paint and
hit-test with. Panels and canvases are then painted parent-first, under
everything they contain, so a container never erases its own descendants.

## The canvas viewport

A canvas has three nested regions:

```
+-- canvas bounds -------------------------------+
| +-- chrome bounds ------------------+ |        |
| | +-- client bounds (viewport) --+  | | scroll |
| | |                              |  | | bar    |
| | |   application painting       |  | |        |
| | +------------------------------+  | |        |
| |   rulers reserve here             | |        |
| +-----------------------------------+ |        |
|            horizontal scrollbar                |
+------------------------------------------------+
```

Scrollbars occupy the outermost edges; non-client strips such as rulers stack
inside them. That ordering is what makes the vertical ruler stop above a
visible horizontal scrollbar rather than run under it.

`wnd` gained one protected hook to express this:

```cpp
// Return the host-relative area left for non-client elements and the
// client after this window's own edge chrome.
virtual rect get_chrome_bounds() const;
```

A plain window returns its complete bounds, so nothing changed for existing
controls. `canvas` returns its bounds minus the visible scrollbar extents.
`get_client_bounds()` is then `reserve_non_client(get_chrome_bounds())`, and
non-client strips are placed inside the same chrome rectangle. One hook keeps
both the client area and the strip geometry consistent.

## Resolving scrollbar visibility

The two axes are not independent: showing a vertical scrollbar narrows the
viewport, which can push the horizontal axis into overflow. The resolution loop
therefore only ever turns visibility *on*, which guarantees it settles:

```cpp
for (int pass = 0; pass < 3; ++pass) {
    const rect chrome = reserve_edges(bounds, vertical_extent, horizontal_extent);
    geometry.viewport = reserve_non_client(chrome);

    const bool horizontal = geometry.horizontal_visible ||
        (policy == scrollbar_policy::automatic &&
         _content.width > geometry.viewport.d.w);
    // ... same for the vertical axis
    if (nothing_changed)
        break;
}
```

A 100×100 canvas with 100×200 of content resolves in three passes: neither
scrollbar, then vertical (height overflows), then horizontal too (the vertical
scrollbar left only 84 pixels of width for 100 pixels of content). The third
pass changes nothing and the loop stops.

## 32-bit content in a 16-bit world

`coord` is `int16_t` and `dim` is `uint16_t`, because they describe on-screen
geometry. A drawing is not on-screen geometry. `canvas_content_bounds` and
`canvas_scroll_position` are therefore signed 32-bit, and a negative origin is
normal — a diagram page can extend left of and above its own origin.

Thirty-two bits is a deliberate middle point. It is far past what `coord` can
express, so a zoomed drawing is never truncated by screen geometry, and it is
still a value every backend, file format, and scripting boundary handles
without a widening conversion.

Overflow is still a real case rather than a theoretical one, because a span of
`UINT32_MAX` added to a negative origin does not fit back into `int32_t`. All
of it is handled by accumulating in a wider type and clamping once:

```cpp
std::int32_t saturating_advance(std::int32_t origin, std::uint32_t delta) {
    const std::int64_t sum = static_cast<std::int64_t>(origin) +
                             static_cast<std::int64_t>(delta);
    return sum > int32_max ? int32_max : static_cast<std::int32_t>(sum);
}
```

That is the whole trick, and it is why the content types are the widest thing
in the control that is still narrower than the accumulator available to it.

Mapping the range onto a pixel track uses floating point. A 32-bit value
converts to `double` exactly, so the only approximation is the pixel track's
own resolution. Both endpoints must still land exactly, so
`position_from_track()` special-cases offset zero and the end of travel rather
than relying on rounding.

## Painting order

`canvas::on_native_paint()` runs three stages against one backend notification:

1. The client viewport, clipped to `get_client_bounds()` intersected with the
   invalid rectangle, emitted as `on_wnd_paint`. The subscriber owns every
   pixel; Native draws nothing under or over it.
2. Non-client strips, through the inherited `wnd::draw_non_client()`.
3. Scrollbar tracks, thumbs, and the corner filler, through
   `theme::draw_scrollbar_part` and `theme::draw_surface`.

The clip in stage 1 is what enforces the contract. A subscriber that ignores
the invalid rectangle and paints its whole content still cannot reach a ruler,
a scrollbar, a sibling, or the parent.

Native never translates the drawer by the scroll position. Paint rectangles and
pointer coordinates stay viewport-local, and the application applies its own
content transform — it is the only party that knows its zoom, and a transform
imposed here would have to be undone by every subscriber that has one.

## Scrollbar interaction

The canvas owns its scrollbar input. `on_native_mouse_click()` hit-tests the
tracks first: a press on a thumb starts a drag and records the grab offset, a
press in the empty track pages toward the pointer by the current viewport span,
and a release ends the interaction. None of those reach `on_mouse_click`,
because a scrollbar press is chrome, not a client click.

Everything outside the tracks falls through to the inherited dispatch
unchanged. Wheel input scrolls the matching axis when it can move and still
emits `on_mouse_wheel` exactly once, so a subscriber that wants to zoom on the
wheel still sees every event.

## Interoperability

Both classes are ordinary borrowed child windows, so they work wherever one is
accepted: as `tab_view` content, as either `split_view` pane, as an `accordion`
body, inside another panel, or as a layout-managed child of any host. A panel
is also a valid parent for every existing control, including another panel and
a canvas. That is what lets a tab page hold more than one control:

```
app_wnd
  status_bar                     (bottom non-client)
  split_view
    accordion                    (left pane)
      icon_view                  (one body per category)
    tab_view                     (right pane)
      canvas                     (one per document tab)
        horizontal ruler         (canvas non-client)
        vertical ruler           (canvas non-client)
        horizontal scrollbar     (canvas chrome)
        vertical scrollbar       (canvas chrome)
```

The rulers and scrollbars reserve canvas space only. They never extend across
the stencil pane or the tab strip, because they are attached to the canvas and
resolved against its bounds.
