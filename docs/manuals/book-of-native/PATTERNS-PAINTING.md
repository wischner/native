# Patterns: Window Painting

This chapter expands Section 6 of the architectural standards. Native uses one
portable drawing interface while allowing each backend to prepare, cache, and
present graphics in the way its platform requires.

For the application-facing operation list and complete window/image examples,
see [Drawing Primitives](DRAWING-PRIMITIVES.md).

## The `gpx` interface

`gpx` is the abstract drawing interface visible to application code. It stores
portable drawing state and defines operations for:

- Foreground and background colors.
- Pen thickness and font selection.
- Clipping and clearing.
- Lines and outlined or filled rectangles.
- Text and images.

Its API accepts only public Native and standard C++ types. Drawing and
state-changing operations return `gpx &` where chaining is useful:

```cpp
graphics.set_ink(native::rgba(20, 40, 80, 255))
        .set_pen(2)
        .draw_line({10, 10}, {100, 60})
        .draw_rect({20, 20, 80, 40});
```

The active font can be measured through the same context before drawing:

```cpp
const native::font_metrics line = graphics.get_font_metrics();
const native::text_metrics run = graphics.measure_text("editable text");
const native::text_metrics caret = graphics.measure_character(U'x');
```

`line.height` supplies line spacing, `run.width` supplies the painted run
bounds, and `caret.advance` supplies cursor movement.

An implementation can map these operations to a native graphics API or to
portable image-buffer drawing. Native handles and cached drawing objects stay
in private backend state.

## Drawing targets

Native uses concrete contexts for two different targets:

- `gpx_wnd` draws into a created window. It borrows the window and uses the
  backend resources associated with that window.
- `gpx_img` draws into an `img`. The image owns its pixel buffer and the
  context borrows that image as its target.

These contexts share the public `gpx` contract, so drawing code can often work
with either target. The difference is important to the backend: a window may
require a paint transaction or presentation step, while an image is an owned
memory buffer.

## Paint-event flow

Calling `invalidate()` starts an asynchronous native repaint request:

```text
invalidate region
       |
       v
backend schedules native repaint
       |
       v
native expose/paint event arrives
       |
       v
backend prepares context and clip
       |
       v
window/control background draw stage, where defined
       |
       v
one synchronous on_wnd_paint emission
       |
       v
attached non-client strips paint
       |
       v
control-owned edge chrome paints
       |
       v
backend presents output and ends painting
```

`invalidate()` must not emit `on_wnd_paint` directly. Toolkits need the freedom
to merge invalid regions and choose when the native drawing surface is valid.
Changing a window's mouse cursor is likewise independent of invalidation:
`wnd::apply_cursor()` updates the native pointer resource without initiating a
paint pass.

For each native paint event, the backend emits one `wnd_paint_event` on the UI
thread. Its two fields are:

- `r`: the invalid client-area rectangle.
- `g`: a borrowed `gpx &` whose concrete window context is `gpx_wnd`.

The last two stages run after the subscriber, in that order.
`wnd::on_native_paint()` emits the signal and then calls the protected
`draw_non_client()`, which paints every visible attached strip over the bounds
`non_client_bounds()` resolved for it. A control that owns edge chrome of its
own overrides `on_native_paint()`, clips the client stage to its viewport,
calls `draw_non_client()`, and paints its chrome last — this is how `canvas`
draws its scrollbars and their corner filler without ever handing them to the
application's paint subscriber.

`app_wnd` and `panel` draw their overridable themed background before calling
the base paint entry point. Their normal empty surface uses the theme's panel
role; editors and item-bearing controls deliberately keep the contrasting
content role. Simple owner-drawn controls dispatch separate
background, border or indicator, text/content, and focus hooks from
`draw_control()`. `split_view` likewise separates its divider background from
its grip. Each base hook draws the complete default for only its stage, and no
later pass repaints that stage after an override returns.

SDL presents a complete invalidated frame while a newly created top-level
window is still hidden, then shows it. This prevents the renderer's initial
black backbuffer from becoming a transient first frame. On destruction it
releases the renderer before its target window.

The ordering matters in one direction only: the subscriber must never be able
to reach the chrome. Clipping the client stage is what enforces that, so a
handler that ignores `r` and paints its whole surface still cannot overwrite a
ruler or a scrollbar track.

## Context lifetime

The graphics reference carried by a paint event is valid only during that
synchronous callback. A handler must not store its address or reference, and
must never delete it.

```cpp
bool handle_paint(native::wnd_paint_event event) {
    event.g.set_clip(event.r)
           .clear(native::rgba(255, 255, 255, 255))
           .set_ink(native::rgba(30, 30, 30, 255))
           .draw_rect(native::rect(10, 10, 120, 40));
    return true;
}
```

The C++ context object may be cached internally, but native resources attached
to the active paint operation can have a shorter lifetime. Treating the event
reference as callback-scoped is therefore required on every backend.

`img::get_gpx()` has a different usage pattern. The image owns its lazily
created context, so callers may use the returned reference while the image
remains alive. The context still borrows its target and must not outlive the
image.

Images can be decoded from PNG or JPEG files and memory, painted through
`get_gpx()`, and encoded again:

```cpp
native::img icon = native::img::load("icon.png");
icon.get_gpx().set_ink({220, 40, 40, 255})
              .draw_rect({1, 1, 12, 12});
icon.save("edited.jpg", 92);

std::vector<std::uint8_t> png =
    icon.encode(native::image_format::png);
```

PNG retains alpha. JPEG output uses the requested quality from 1 through 100
and has no alpha channel. File-backed image operations accept
`std::filesystem::path`.

## Invalid rectangles and clipping

Painting must honor both the invalid rectangle and the currently active clip.
A handler should restrict its work to the invalid region when practical. A
backend must also ensure drawing cannot escape the native update region.

The invalid rectangle is expressed in the same public client coordinates used
by window drawing. Backends convert native update regions before emission.

Clipping is stateful. Code that temporarily changes a clip or other drawing
state should restore it before returning when a caller is expected to continue
using the same context. Theme primitives have a stronger guarantee: they must
preserve all caller-visible `gpx` state.

Native view state also belongs to the caller. Haiku saves and restores the
`BView` state for each window-graphics primitive, including clipping, color,
pen, font, and drawing mode. Foreground and pen state are established on each
call because native widgets and `BControlLook` can change them outside the
graphics cache. Text draws over the actual destination rather than using an
unrelated low-color background for antialiasing.
Haiku controls stay locally hidden while their peer bindings are registered;
the later `show()` exposes them, so a window thread cannot enter their custom
paint path before a graphics target exists. Lists also defer direct native
selection painting to the full-control invalidation path.

## Repainting persistent content

A window is not a durable bitmap. Native systems can ask it to repaint after
being uncovered, resized, restored, or moved between displays. Application
code should store model state and redraw from that state during every relevant
paint event.

For example, a painting application stores its lines in a collection. Mouse
input changes the collection and calls `invalidate()`; the paint handler draws
the collection. It does not rely on pixels from an earlier callback remaining
on the screen.

## Backend responsibilities

Each backend must:

1. Translate native expose or paint notifications into one synchronous public
   paint event.
2. Compute the invalid client rectangle in Native coordinates.
3. Prepare a valid `gpx_wnd` target and active clip.
4. Handle native begin/end-paint requirements.
5. Present buffered or renderer output when the toolkit requires it.
6. Release native graphics resources when the window is destroyed.
7. Keep native renderers, handles, buffers, and cached drawing objects out of
   public classes.

Backends can differ in buffering and presentation, but handlers must observe
the same event, coordinate, clipping, and lifetime rules.
