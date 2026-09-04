# Panels and canvases

Two controls carry no semantics of their own. A `panel` groups and lays out
other controls. A `canvas` gives its whole client area to your painting and
supplies scrollbars around it. Reach for a panel when a region needs to hold
several controls, and for a canvas when your program draws the content itself.

They are separate classes on purpose. A panel is not a drawing surface, and a
canvas is not a container.

## Grouping controls with a panel

A panel is an empty child window. It adds no border, caption, focus ring, or
padding, and it shows the platform's ordinary control-host background. Give it
a layout and assign children to it exactly as you would to a window:

A derived panel may override the protected `draw_background()` stage to
replace only that host background. The inherited paint signal, non-client
strips, child lifecycle, and layout continue to run normally.

```cpp
native::panel page(0, 0, 400, 300);
native::button command("Run");
native::text_edit path("", native::text_edit_mode::single_line);

page.set_parent(&window);
command.set_parent(&page);
path.set_parent(&page);

auto grid = std::make_unique<native::grid_layout_manager>();
(*grid) << native::row(native::pixels(28))
        << native::row(native::star())
        << native::column(native::pixels(120))
        << native::column(native::star())
        << native::cell(command, 0, 0)
        << native::cell(path, 0, 1);
page.set_layout(std::move(grid));
```

Order does not matter. A child assigned before the layout and one assigned
after it are both registered exactly once, and each is placed as soon as the
panel has a layout to place it with.

A panel borrows its children. It never owns or deletes them. Destroying a
panel destroys the native resources of any created children, but the C++
objects stay yours.

## Creating panel children

A panel supplies the native parent; it does not create or show children for
you. Create them from the panel's `on_wnd_create` handler, which runs once the
panel's own resource exists:

```cpp
page.on_wnd_create.connect([&] {
    command.create();
    command.show();
    path.create();
    path.show();
    return true;
});

page.create();
page.show();
```

Creating a child before its panel, or with no parent assigned, reports an error
the same way every other control does.

Panels nest. A panel inside a panel keeps its own layout and its own children,
and reparenting follows the usual rules — including the rejection of a cycle.

## Panels as tab and split content

`tab_view` and `split_view` each borrow one child window per slot. A panel is
what you use when a slot needs more than one control:

```cpp
native::panel general;
native::panel advanced;
native::tab_view tabs(20, 20, 420, 260);
tabs.add_item("General", general);
tabs.add_item("Advanced", advanced);
```

Each page now holds a full layout of its own.

## Drawing with a canvas

A canvas hands you its client area and stays out of the way. It uses the paint
and pointer signals `wnd` already defines:

```cpp
native::canvas drawing(0, 0, 400, 300);
drawing.set_parent(&window);

drawing.on_wnd_paint.connect([&](native::wnd_paint_event event) {
    event.g.clear(native::rgba(255, 255, 255, 255))
        .set_ink(native::rgba(40, 80, 160, 255))
        .draw_rect(native::rect(10, 10, 120, 80), true)
        .draw_text("content", native::point(14, 100));
    return true;
});

drawing.on_mouse_click.connect([&](native::mouse_event event) {
    // event.position is canvas-local on every backend.
    return true;
});
```

The drawer is valid only for the duration of the callback. Do not store it, and
do not call `get_gpx()` during construction.

Your drawing is clipped to the viewport. You cannot paint over a ruler, a
scrollbar, a sibling control, or the parent window, even if you ignore
`event.r` and paint the whole surface. Native does not clear over your output
after the callback either — the pixels are yours.

## Scrolling a canvas

Describe your content once, in your own coordinates, and read the scroll
position back when you paint:

```cpp
drawing.set_content_bounds({-4000, -3000, 12000, 9000});

drawing.on_wnd_paint.connect([&](native::wnd_paint_event event) {
    const auto scroll = drawing.get_scroll_position();
    // Offset your drawing by scroll.x and scroll.y.
    return true;
});

drawing.on_scroll.connect([](native::canvas_scroll_position position) {
    return true;
});
```

Content bounds are signed 32-bit values, not screen coordinates. A negative
origin is normal: a page that extends left of and above its own origin is
described exactly, and a drawing far larger than any window is not truncated.

The scroll position is the content coordinate shown at the viewport's leading
edge. Native clamps it on each axis to
`[origin, max(origin, origin + span - page)]`, so you can pass any value and
get a usable one back. Empty content reports position zero.

`set_scroll_position()` is programmatic and silent, like every other Native
setter. `on_scroll` emits only for user-originated scrolling — a drag, a page
click, a wheel notch — and only when the effective position actually changed.

Native does not transform your drawer by the scroll position. Paint rectangles
and pointer coordinates stay viewport-local, and you apply your own transform.
That keeps the canvas out of your zoom and content model.

## Scrollbar policy

Each axis has its own policy, and both default to `automatic`:

```cpp
drawing.set_vertical_scrollbar_policy(native::scrollbar_policy::always);
drawing.set_horizontal_scrollbar_policy(native::scrollbar_policy::never);
```

- `automatic` shows a scrollbar only when the content does not fit.
- `always` reserves the themed extent and draws an inactive scrollbar disabled.
- `never` neither reserves nor draws one, and still allows programmatic
  scrolling.

Showing one scrollbar narrows the other axis, so an automatic decision on one
axis can turn the other on. Native resolves both together and reports the
settled answer through `get_horizontal_scrollbar_visible()` and
`get_vertical_scrollbar_visible()`.

Scrollbar extents, minimum thumb size, and appearance come from the active
theme. There is nothing to size or color yourself.

## Rulers on a canvas

A canvas can own inherited non-client strips. Rulers attached to a canvas
reserve canvas space only, never the space of a surrounding tab strip or split
pane:

```cpp
native::ruler horizontal(drawing, native::ruler_orientation::horizontal);
native::ruler vertical(drawing, native::ruler_orientation::vertical);
horizontal.set_visible(true);
vertical.set_visible(true);
```

Scrollbars take the outermost edges and rulers sit inside them, so the
horizontal ruler stops before a visible vertical scrollbar and the vertical
ruler stops above a visible horizontal one. `get_client_bounds()` reports what
is left after both.

## A document workspace

Panels and canvases compose with the containers you already have:

```cpp
native::accordion stencils;
native::canvas page_one;
native::canvas page_two;
native::tab_view documents;
native::split_view workspace(
    stencils, documents, native::split_orientation::horizontal);

documents.add_item("Drawing 1", page_one);
documents.add_item("Drawing 2", page_two);
```

Selecting a tab shows and paints that page's canvas and no other. Moving the
splitter updates the tab view and the selected canvas, and canvas paint and
pointer coordinates stay canvas-local afterwards. Scrolling one page changes
only that page.
