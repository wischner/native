# Patterns: Window Painting

This chapter expands Section 6 of the architectural standards. Native uses one
portable drawing interface while allowing each backend to prepare, cache, and
present graphics in the way its platform requires.

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
one synchronous on_wnd_paint emission
       |
       v
backend presents output and ends painting
```

`invalidate()` must not emit `on_wnd_paint` directly. Toolkits need the freedom
to merge invalid regions and choose when the native drawing surface is valid.

For each native paint event, the backend emits one `wnd_paint_event` on the UI
thread. Its two fields are:

- `r`: the invalid client-area rectangle.
- `g`: a borrowed `gpx &` whose concrete window context is `gpx_wnd`.

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
