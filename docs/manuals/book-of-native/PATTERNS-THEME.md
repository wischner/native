# Patterns: Custom And Themed Drawing

This chapter expands Section 7 of the architectural standards. Custom drawing
that represents a familiar control should use the public `native::theme`
facade instead of copying the appearance of one operating system.

Rulers have separate `surface_kind::ruler` and `ruler_corner` paper roles.
Their default treatment is header and panel paper respectively; OpenMotif
uses the actual native menu-bar background for both, with menu-foreground
ticks and labels. Table and accordion
headers keep their original colors. Motif table alternate rows blend 10%
of the native data foreground into its background; grid lines blend 65%,
so both remain distinguishable in CDE's dark data palette.

For a table of the available control parts, state fields, and application
examples, see [Drawing Primitives](DRAWING-PRIMITIVES.md#themed-control-primitives).

## Semantic drawing

macOS follows the same stock-versus-derived distinction as Windows. Ordinary
buttons/checks/radios call AppKit's renderer. Table and outline rows use native
`NSTableCellView` text and image subviews, and headers use `NSTableHeaderCell`.
`NSCollectionView` items compose `NSImageView`, `NSTextField` and a native
selection box. A stock accordion uses its native disclosure stack without
calling the portable accordion painter. Explicitly derived controls retain
the protected stages; theme primitives remain available for custom visuals.

Motif list primitives use the native list's `XmFontList` and `XmStringDraw`,
not the button font. Content foreground and background are read as a pair
from list resources, preserving CDE's light text on slate data surfaces.
Radio indicators honor `XmNindicatorType` and the display's
`XmNenableToggleVisual`, selecting Motif's own circle or diamond primitive.

Windows stock buttons, checks, radios, trees, icon views and table headers
retain native control painting, metrics and interaction. A Common Controls v6
manifest enables visual styles. Theme samples use matching UxTheme button
parts (with classic GDI fallback) and native list font metrics. Derived control
types retain owner/custom-draw stages for application overrides. Virtual table
group rows retain an adapter because Windows does not support group view
with `LVS_OWNERDATA`.

X11/Athena uses its native flat two-color control resources. Its table
alternate-row color equals the white content surface instead of synthesizing
gray stripes. Collection image drawing thresholds scaled pixels to black or
white, including transparency, so interpolated icons do not introduce color
or gray fringes. This happens only in X11 collection windows: source images,
image graphics and application canvases retain full RGBA color. Accordion
headers draw a top separator; selected tabs use white paper, black labels and
an open page join. Xaw alerts receive locally copied monochrome bitmap assets
through their native icon resource.

The GEMix palette is monochrome. Its custom radio primitive uses the same
circle and selected dot as the live radio; headers retain complete outlines,
status surfaces carry a top separator, and scrollbar parts use flat GEM-style
frames, stippled tracks, and arrow buttons. Offscreen stock text reads the same
immutable GEM bitmap font resource as VDI window text. Unsupported Unicode is
substituted predictably, including a printable three-dot ellipsis, before both
measurement and rendering.

The theme API describes what is being drawn rather than how one toolkit draws
it. Its primitives include complete buttons, checks, radios, lists, menu bars
and menu items, popup frames, and list items. Reusable advanced parts cover
surfaces, selections, focus, disclosures, separators, and scrollbars. The
facade also provides compact caption buttons and semantic separators. The
backend decides how each primitive is decomposed for its native painter.

Theme primitives paint only. Live controls, including `accordion` and
`icon_view`, and `tree_view`, remain `wnd` subclasses and use native widgets
when available.

Every public control is inheritable. Its protected virtual painting stages are
the owner-draw contract: simple controls dispatch background, border or
indicator, text/content, and focus stages from their complete control entry
point, while collections also split rows, cells, images, disclosures, and
scrollbars. Windows and panels expose their surface background, and split
views separate the splitter background from its grip. The default pixels are
drawn inside each base virtual method. A derived implementation can therefore
replace a part, or add to it by calling base:

```cpp
void result_table::draw_cell_content(
    native::gpx &graphics,
    native::theme &appearance,
    native::table_row_id row,
    std::size_t model_row,
    const native::table_column &column,
    const native::table_cell &cell,
    const native::rect &bounds,
    const native::theme::state &state) {
    native::table_view::draw_cell_content(
        graphics, appearance, row, model_row,
        column, cell, bounds, state);
    // Draw an application-specific decoration after native content.
}
```

Renderers invoke these stages directly; they do not draw a default and then
call an owner hook. Native backends enter them through the platform's normal
custom-draw, cell/view, expose, or repaint facility, keeping native input and
metrics while allowing the same derived C++ class on every backend.

A complete themed check or radio and the corresponding live control stages
share one visual contract: the panel background, indicator geometry, stock
control font, semantic colors, and focus treatment must agree. SDL's emulated
controls follow this rule directly so theme samples compare pixel-for-pixel
with ordinary controls.
Haiku supplies its own checkbox drawing stages entirely within its platform
backend. Its indicator and label use `BControlLook` through the Haiku theme,
with separate clips preserving the owner-draw stage boundaries. The default
checkbox renderer used by SDL2 and other backends is not compiled into Haiku;
only the public C++ property/event model is shared.

Interaction is also expressed semantically:

```cpp
native::theme::state state;
state.hot = true;
state.pressed = false;
state.selected = false;
state.disabled = false;
state.focused = true;
state.active = true;
```

Backends interpret the same state using their native theme facilities or a
portable fallback. Application code does not choose native colors, bevel
widths, widget classes, or theme-part identifiers.

## Borrowed graphics context

A theme object is created by the active backend around a borrowed `gpx &`:

```cpp
bool handle_paint(native::wnd_paint_event event) {
    auto painter = native::theme::create(event.g);
    native::theme::state state;
    state.hot = pointer_inside;
    state.pressed = button_down;

    painter->draw_button(
        native::rect(20, 20, 120, 32),
        "Continue",
        state);
    return true;
}
```

The returned object implements the abstract `theme` interface for the active
platform or toolkit and does not own the context. When it is created around a
paint-event context, both are callback-scoped and must not be stored.

## Native rendering first

A backend should use a native theme or toolkit primitive when that primitive
can draw correctly into the target. Examples include Windows theme drawing,
Motif `XmeDraw*` helpers, XView OLGX primitives, WINGs relief and indicator
resources, and the equivalent AppKit or BeAPI facilities.

Native rendering is preferred because it follows the user's current colors,
metrics, accessibility settings, theme version, and interaction conventions.
It should not be approximated in shared code when the platform already exposes
the correct operation.

OPEN LOOK button primitives retain XView's intrinsic text-button height and
left-aligned labels; allocating a taller layout cell does not stretch a native
text button. Exclusive choices use OLGX's rectangular choice-item primitive,
not an oblong command button or a circular radio from another toolkit.
OPEN LOOK rebuilds its complete off-screen scene for a partial exposure and
copies only the exposed rectangle. This keeps OLGX geometry and colors stable
instead of switching to a different fallback when an exposed rectangle cuts
through a control. Explicit invalidations are coalesced into the next notifier
turn, avoiding painting during construction or repeated layout setters.

## Portable fallback

A suitable native primitive is not always available. It may be unable to draw
into an image, the toolkit may not expose the requested part, or the selected
backend may emulate that control.

In that case, the backend implementation uses portable `gpx` operations. The
fallback remains in that platform or toolkit directory and must:

- Support the same semantic states.
- Remain legible and usable.
- Use backend-derived palette and metrics where available.
- Work for both native-window and image targets when required.
- Avoid embedding one platform's appearance in shared code.

Reporting that native drawing is unavailable does not remove the obligation to
draw the primitive. Every backend must provide a usable result.

## Palette and metrics

The facade exposes every backend-selected control dimension through a named
getter. Fixed shapes use getters such as `get_button_height()`,
`get_text_edit_height()`, `get_tab_height()`, `get_status_bar_height()`, and
`get_disclosure_size()`. Collection geometry includes row, header, tree,
icon-view, border, and padding getters. Linear shapes expose both their fixed
thickness and an orientation-aware size helper:

```cpp
const native::size vertical_scroll = appearance->get_scrollbar_size(
    native::scrollbar_orientation::vertical, available_height);
const native::size bottom_status =
    appearance->get_status_bar_size(available_width);
```

`get_separator_size()`, `get_scrollbar_size()`, and `get_ruler_size()` put the
native extent on the correct axis. Their length argument is supplied by the
layout because a theme cannot determine the available window dimension.

Named color getters cover the complete semantic palette. Buttons and menus
provide normal, disabled, hot, and pressed foreground/background colors plus
their borders, highlights, and shadows. Content, alternating content,
selection, inactive selection, separators, and focus indicators have matching
getters. Every color getter returns an opaque `rgba`; in particular,
`get_content_alternate_background_color()` resolves a backend's optional
alternate-row sentinel into a usable derived color.

The panel color is the normal application-window and container background.
The content color is reserved for editors and item surfaces such as lists,
trees, icon grids, and tables. Painted status parts use panel color with theme
highlight/shadow edges instead of appearing as white editable fields.
Combo arrow buttons use that content color with a compact filled mark. Portable
collection and table scrollbars share one classic composition: matching raised
decrement/increment buttons, a page trough, and a gripped thumb. Disclosure
size remains a backend metric so SDL2 and other compact themes can avoid bulky
hierarchy marks.
Athena tables, icon grids, trees and canvases instead own stock Xaw
`Scrollbar` children. Their stippled thumbs and pointer interaction are
native; the theme suppresses duplicate portable scrollbar parts only in
those window contexts. Standalone theme primitives and image drawing retain
the painted fallback. Menu titles and popups have individual borders without
an additional full-width menu-bar rule.
Haiku icon grids replace fallback scrollbar parts with `BControlLook` drawing
and read the system knob preference. Both table modes instead own actual
`BScrollBar` widgets. Their extent includes the final pixel of Haiku's
inclusive native rectangle. The virtual-table header uses the same native
button-background recipe as `BColumnListView`. Haiku sets
`table_fit_visible_rows` to false so table rows keep their native pitch,
rather than compressing a partially visible page. Its accordion headers omit
the generic blue focus rectangle; the disclosure mark indicates expansion.
Native checkboxes invalidate again after mouse tracking ends, so a repaint
during a change callback cannot leave an unchecked indicator depressed.

```cpp
event.g.clear(appearance->get_content_background_color())
       .set_ink(appearance->get_content_foreground_color());
appearance->draw_selection(row, native::selection_shape::row, state);
```

`defaults()` and `native_palette()` remain available for code that needs a
single snapshot. New custom drawing should prefer named getters, which make
the meaning of each value explicit and guarantee opaque color results.

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

The implementation should use `gpx::save_state()`, perform the native or
fallback drawing, and let the guard restore state before returning. A caller must not
need backend-specific repair code after drawing a theme primitive.

## Adding a theme primitive

Adding a public primitive changes the cross-platform contract. The work is:

1. Add the semantic operation and any portable state to the abstract interface.
2. Define behavior for every state combination that the operation accepts.
3. Implement native drawing where the backend can support the target.
4. Implement or reuse a portable `gpx` fallback everywhere else.
5. Preserve graphics state in every path, including failures and fallbacks.
6. Update every supported backend and add visual or automated coverage where
   practical.

If the primitive is part of a new public `wnd` subclass, that subclass also
needs the complete lifecycle, event translation, and drawing support described
in [Windows And App Windows](PATTERNS-WINDOWS.md).
