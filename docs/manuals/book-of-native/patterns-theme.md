# Patterns: Custom And Themed Drawing

This chapter expands Section 7 of the architectural standards. Custom drawing
that represents a familiar control should use the public `native::theme`
facade instead of copying the appearance of one operating system.

## Semantic drawing

The theme API describes what is being drawn rather than how one toolkit draws
it. Its primitives include button faces, frames and labels, menu bars and menu
items, popup frames, and list items.

Interaction is also expressed semantically:

```cpp
native::theme::state state;
state.hot = true;
state.pressed = false;
state.selected = false;
state.disabled = false;
```

Backends interpret the same state using their native theme facilities or a
portable fallback. Application code does not choose native colors, bevel
widths, widget classes, or theme-part identifiers.

## Borrowed graphics context

A theme object wraps a borrowed `gpx &`:

```cpp
bool handle_paint(native::wnd_paint_event event) {
    native::theme painter(event.g);
    native::theme::state state;
    state.hot = pointer_inside;
    state.pressed = button_down;

    painter.draw_button(
        native::rect(20, 20, 120, 32),
        "Continue",
        state);
    return true;
}
```

The theme object does not own the context. When it is constructed around a
paint-event context, both are callback-scoped and must not be stored.

## Native rendering first

A backend should use a native theme or toolkit primitive when that primitive
can draw correctly into the target. Examples include Windows theme drawing,
Motif `XmeDraw*` helpers, and the equivalent AppKit or BeAPI facilities.

Native rendering is preferred because it follows the user's current colors,
metrics, accessibility settings, theme version, and interaction conventions.
It should not be approximated in shared code when the platform already exposes
the correct operation.

## Portable fallback

A suitable native primitive is not always available. It may be unable to draw
into an image, the toolkit may not expose the requested part, or the selected
backend may emulate that control.

In that case, the backend uses portable `gpx` operations. The fallback must:

- Support the same semantic states.
- Remain legible and usable.
- Use backend-derived palette and metrics where available.
- Work for both native-window and image targets when required.
- Avoid embedding one platform's appearance in shared code.

Reporting that native drawing is unavailable does not remove the obligation to
draw the primitive. Every backend must provide a usable result.

## Palette and metrics

The facade exposes backend-selected control metrics and a native palette. These
values provide menu heights, padding, popup dimensions, and state-sensitive
colors to portable rendering code.

Defaults are only a fallback. A backend should obtain colors, fonts, spacing,
and dimensions from its toolkit whenever possible. Hard-coded values copied
from another platform lead to controls that look wrong and can become unusable
under dark, high-contrast, or enlarged themes.

## Preserve drawing state

A theme primitive borrows a context that its caller may continue using. It
must preserve caller-visible `gpx` state, including:

- Ink and paper colors.
- Pen thickness.
- Selected font.
- Active clipping rectangle.

The implementation should capture the incoming state, perform the native or
fallback drawing, and restore the state before returning. A caller must not
need backend-specific repair code after drawing a theme primitive.

## Adding a theme primitive

Adding a public primitive changes the cross-platform contract. The work is:

1. Add the semantic operation and any portable state to the shared interface.
2. Define behavior for every state combination that the operation accepts.
3. Implement native drawing where the backend can support the target.
4. Implement or reuse a portable `gpx` fallback everywhere else.
5. Preserve graphics state in every path, including failures and fallbacks.
6. Update every supported backend and add visual or automated coverage where
   practical.

If the primitive is part of a new public `wnd` subclass, that subclass also
needs the complete lifecycle, event translation, and drawing support described
in [Windows And App Windows](patterns-windows.md).
