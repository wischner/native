# Drawing Primitives

This chapter is the application-facing reference for drawing with Native. It
covers the primitives shared by window and image graphics contexts, themed
control parts, image input and output, and the text measurements needed by
editors and other precisely laid-out interfaces.

The examples include `<native.h>`. Individual library components can instead
include `<native/graphics.h>`, `<native/font.h>`, and `<native/theme.h>`.

## Choose a drawing target

Every primitive is called through `native::gpx`, but the context has one of two
targets:

| Target | Obtain the context from | Lifetime |
| --- | --- | --- |
| Window | `wnd_paint_event::g` | Borrowed; valid only during that paint callback |
| RGBA image | `img::get_gpx()` | Owned by the image; valid while the image lives |

Drawing code that only needs `gpx` can be reused for both:

```cpp
void draw_badge(native::gpx &g, const native::rect &bounds) {
    g.set_ink(native::rgba(34, 92, 160, 255))
     .draw_rect(bounds, true)
     .set_ink(native::rgba(255, 255, 255, 255))
     .draw_text("Native", native::point(bounds.x1() + 8,
                                        bounds.y1() + 5));
}
```

Window contexts may use a platform painter, while image contexts render into
owned RGBA pixels. The public operations and coordinate system remain the
same.

## Graphics state

A graphics context is stateful. These values affect later operations:

| Operation | Meaning |
| --- | --- |
| `set_ink(rgba)` / `get_ink()` | Foreground color used by lines, rectangles, and text |
| `set_paper(rgba)` / `get_paper()` | Current background color state |
| `set_pen(uint8_t)` / `get_pen()` | Line and outline thickness in pixels; use a positive value |
| `set_font(const font_t &)` / `get_font()` | Borrowed font used for drawing and measurement |
| `set_clip(rect)` / `get_clip()` | Half-open rectangle that restricts subsequent drawing |

State setters return `gpx &`, so related operations can be chained. A selected
font is not copied or owned by the context; it must remain alive while it is
selected. If no font is selected, the stock system font is used.

The clip is always active. `clear(color)` fills the current clip, not
necessarily the entire target. A new image context starts with its clip set to
the full image. During window painting, restrict drawing to the invalid area
reported by the event.

Code that borrows a context should restore any state that its caller expects
to reuse. A move-only `gpx_state` from `save_state()` restores ink, paper,
pen, font, and clip when its scope exits. Theme operations use the same
contract.

## Geometric and content primitives

The common drawing operations are:

| Primitive | Result |
| --- | --- |
| `clear(color)` | Fill the active clip with `color` |
| `draw_line(from, to)` | Draw a line in the current ink and pen thickness |
| `draw_rect(bounds)` | Draw the outline of a half-open rectangle |
| `draw_rect(bounds, true)` | Fill a rectangle with the current ink |
| `draw_ellipse(bounds, filled)` | Draw an outlined or filled ellipse |
| `draw_polyline(points)` | Connect an ordered sequence of points |
| `draw_polygon(points, filled)` | Close and optionally fill point geometry |
| `draw_text(text, position)` | Draw UTF-8 text from a top-left position with the current font and ink |
| `draw_text(text, bounds, layout)` | Draw clipped, aligned, optionally ellipsized text |
| `draw_img(source, destination)` | Draw the complete source image at a point or scaled rectangle |
| `draw_img(source, source_rect, destination, filter)` | Crop and scale with nearest or linear filtering |

Scaled image drawing preserves straight-alpha RGBA and validates that a crop
lies inside the source. Linear is the normal artwork filter; nearest preserves
pixel-art edges. Rotation, opacity modulation, and affine transforms are not
part of this interface.

Native rectangles are half-open: the left and top edges are included, while
`x2()` and `y2()` are excluded. See
[Geometry And Type Conventions](PATTERNS-GEOMETRY.md) for all coordinate and
color rules.

```cpp
void draw_scene(native::gpx &g, const native::img &icon) {
    auto saved = g.save_state();
    g.clear(native::rgba(246, 246, 246, 255))
     .set_pen(2)
     .set_ink(native::rgba(45, 45, 48, 255))
     .draw_line(native::point(12, 18), native::point(210, 18))
     .draw_rect(native::rect(12, 30, 198, 72))
     .draw_img(icon, native::rect(20, 38, 32, 32))
     .draw_text("Ready",
                native::rect(72, 38, 120, 32),
                {native::text_align::start,
                 native::text_valign::center,
                 native::text_overflow::ellipsis,
                 true});
}
```

## Window drawing

Draw a window only while handling its paint event. Keep durable content in the
application model, call `invalidate()` after the model changes, and reconstruct
the visible result when the backend emits `on_wnd_paint`.

```cpp
bool handle_paint(native::wnd_paint_event event) {
    event.g.set_clip(event.r)
           .clear(native::rgba(250, 250, 250, 255));

    draw_badge(event.g, native::rect(16, 16, 96, 28));
    return true;
}
```

Do not retain `event.g`. Some backends attach native resources to it only for
the duration of the current paint transaction. The full repaint and
invalidation contract is described in [Window Painting](PATTERNS-PAINTING.md).

## Images and image contexts

`native::img` owns a fixed-size, contiguous RGBA pixel buffer. Dimensions must
be non-zero and fit positive Native coordinates. Use `w()` and `h()` for its
dimensions and `pixels()` for row-major pixel access:

```cpp
native::img canvas(320, 180);
native::gpx &g = canvas.get_gpx();

g.clear(native::rgba(255, 255, 255, 0));
draw_badge(g, native::rect(12, 12, 96, 28));

native::rgba first = canvas.pixels()[0];
canvas.pixels()[10 * canvas.w() + 20] =
    native::rgba(255, 0, 0, 255);
```

Call `clear` before first use when a specific initial background is required.
`get_gpx()` creates the image context lazily and returns the same context on
later calls. Its state, including its clip, therefore persists between calls.

### Load and decode

PNG and JPEG can be read from either a file or an encoded memory buffer:

```cpp
native::img from_file = native::img::load("photo.jpg");

native::img from_memory = native::img::decode(
    encoded_bytes.data(), encoded_bytes.size());
```

Decoding detects the format from the data, not the filename. Decoded pixels
are RGBA. PNG alpha is retained; JPEG pixels are opaque. File paths use
`std::filesystem::path`; string literals remain convenient because they
construct paths directly.

### Encode and save

Encoding can return an owned byte vector or write a file:

```cpp
std::vector<std::uint8_t> png =
    canvas.encode(native::image_format::png);

std::vector<std::uint8_t> jpeg =
    canvas.encode(native::image_format::jpeg, 92);

canvas.save("preview.png");
canvas.save("preview.jpg", 92);
```

`save` selects PNG for `.png` and JPEG for `.jpg` or `.jpeg`, ignoring case.
JPEG quality is an integer from 1 through 100 and defaults to 90. JPEG does not
store alpha; use PNG when transparency must survive a round trip.

Invalid dimensions, empty encoded input, unsupported extensions, malformed
images, codec failures, and file failures are reported with standard
exceptions. Image objects own their pixels and context and are not copyable.

## Fonts

Native distinguishes semantic stock fonts from portable fonts. Stock fonts
come from the active platform or toolkit and provide the local look and feel:

```cpp
const native::font_t &body =
    native::font_t::stock(native::font_role::system);
const native::font_t &code =
    native::font_t::stock(native::font_role::fixed);
const native::font_t &labels =
    native::font_t::stock(native::font_role::icon_label);
```

The remaining stock roles are `title`, `small`, and `control`. Stock fonts are
borrowed process-lifetime objects. Several roles may select the same native
face when a toolkit does not provide distinct choices.

Use `enumerate_installed()` to populate a font picker. Each
`font_description` contains portable family, style, face name, weight,
italic/fixed-pitch flags, `std::filesystem::path`, and collection face index:

```cpp
std::vector<native::font_description> installed =
    native::font_t::enumerate_installed();
```

Enumeration describes the current machine; it does not make rendering
portable. To select a discovered face, pass its path and face index to
`from_file()`:

```cpp
native::font_t face = native::font_t::from_file(
    installed.front().path, 16, installed.front().face_index);
```

Applications can instead create the same face from encoded bytes. The bytes
are copied before `from_memory()` returns, and both creation paths use the
same shared TrueType validation, UTF-8 layout, measurement, kerning, and
alpha-rasterization path on every backend:

```cpp
native::font_t embedded = native::font_t::from_memory(
    font_bytes.data(), font_bytes.size(), 16, 0);
```

Both factories return an invalid, move-only `font_t` for malformed data, an
invalid size, or an unavailable collection face. Check `valid()` before
selection. A graphics context borrows its selected font, so the font must
outlive every measurement and drawing operation that uses it.

## Text and character measurement

Text must be measured with the same font that will draw it. Measurements are
available directly from a `font_t`, or through `gpx` for its currently selected
font:

| Result | Fields | Use |
| --- | --- | --- |
| `font_metrics` | `ascent`, `descent`, `leading`, `height`, `max_advance` | Line layout and caret height |
| `text_metrics` | `width`, `height`, `advance` | Painted bounds and cursor movement |

Use `measure_text(utf8)` for a run and `measure_character(code_point)` for one
Unicode character. `width` is the horizontal painted extent; `advance` is the
amount by which the next cursor position moves.

```cpp
void draw_editor_line(native::gpx &g,
                      const std::string &text,
                      std::size_t caret_byte) {
    const native::point origin(8, 8);
    const std::string prefix = text.substr(0, caret_byte);
    const native::text_metrics before = g.measure_text(prefix);
    const native::font_metrics face = g.get_font_metrics();

    const native::coord caret_x = static_cast<native::coord>(
        origin.x + before.advance);
    const native::coord caret_bottom = static_cast<native::coord>(
        origin.y + face.height);

    g.draw_text(text, origin)
     .draw_line(native::point(caret_x, origin.y),
                native::point(caret_x, caret_bottom));
}
```

In this example, `caret_byte` must lie on a UTF-8 boundary. A full editor must
also decide how it handles grapheme clusters, selection, shaping, and line
breaking; those policies are above the primitive measurement API.

## Themed control primitives

Use Native window/control classes when an actual interactive control is
needed. They own native lifecycle, input, focus, and accessibility behavior.
Use `native::theme` when a custom-drawn surface needs a familiar control part
or platform look.

A theme draws visuals only. The caller still owns layout, hit testing,
interaction state, invalidation, focus behavior, and event handling.

The interactive selection controls are:

| Control | State and signal |
| --- | --- |
| `native::check` | `get_checked()` / `set_checked()` and `on_change(bool)` |
| `native::radio` | `get_selected()` / `set_selected()` and `on_change(bool)`; sibling radios are exclusive |
| `native::list` | UTF-8 items, a single selected index, and `on_selection_change(int)` |

```cpp
native::check remember("Remember choice", 16, 16, 160, 24);
native::radio compact("Compact", 16, 48, 120, 24);
native::radio detailed("Detailed", 16, 76, 120, 24);
native::list choices(
    std::vector<std::string>{"First", "Second", "Third"},
    160, 16, 150, 90);

remember.set_parent(&window);
compact.set_parent(&window);
compact.set_selected(true);
detailed.set_parent(&window);
choices.set_parent(&window);
choices.set_selected_index(0);
```

Create and show them only after their parent window is created. Property
setters do not emit user-action signals. Native activations update the cached
state and then emit the signal.

Create the active backend theme around the target context:

```cpp
auto controls = native::theme::create(g);
```

The theme borrows `g`, so it must not outlive the context. It uses native
painters where they can draw into the target and a backend-specific emulation
otherwise. On OPEN LOOK window targets, the backend uses OLGX with the same
XView font and control color map as native Panel items. On Window Maker
targets, it uses WINGs relief drawing, screen colors and fonts, and native
check/radio indicator pixmaps. The same calls can
therefore be used for window and image targets.

### Control states

`native::theme::state` describes the meaning of the current visual state:

| Field | Meaning |
| --- | --- |
| `hot` | The pointer is over the element |
| `pressed` | The element is actively depressed |
| `selected` | The item is the current or selected item |
| `disabled` | The element cannot currently be activated |
| `focused` | The element carries keyboard focus |
| `active` | The containing window has active selection emphasis |

All fields except `active` default to `false`; `active` defaults to `true`.
Overloads without a state argument draw the default state.

### Control drawing operations

| Primitive | Draws |
| --- | --- |
| `draw_button(bounds, text, state)` | A complete push button |
| `draw_menu_bar(bounds)` | An empty menu-bar background and edges |
| `draw_menu_title(bounds, text, state)` | One title in a menu bar |
| `draw_popup_frame(bounds)` | A popup-menu background and frame |
| `draw_menu_item(bounds, text, state)` | One popup-menu item |
| `draw_list_item(bounds, text, state)` | One list item |
| `draw_check(bounds, text, state)` | A complete check; `state.selected` is checked |
| `draw_radio(bounds, text, state)` | A complete radio; `state.selected` is chosen |
| `draw_list(bounds, items, selected_index, state)` | A framed single-selection list |
| `draw_text_edit_frame(bounds, state)` | An empty text-edit frame; `state.selected` is focused |
| `draw_surface(bounds, kind, state)` | A panel, content, inset, popup, header, or table-header surface |
| `draw_selection(bounds, shape, state)` | A row or tile selection background |
| `draw_focus(bounds, state)` | A keyboard-focus indicator |
| `draw_disclosure(bounds, disclosure, state)` | A collapsed or expanded indicator |
| `draw_sort_indicator(bounds, direction, state)` | A native table sort indicator |
| `draw_caption_button(bounds, kind, state)` | A compact close, pin, or unpin button |
| `draw_separator(bounds, orientation)` | A native-looking separator |
| `draw_scrollbar_part(bounds, orientation, part, state)` | A track, thumb, or step part |

The caller supplies the rectangles. `defaults()` returns backend-selected
values for menu, header, disclosure, icon-grid, focus,
separator, and scrollbar geometry as well as established control dimensions.
`native_palette()` exposes backend-selected colors for a portable composition,
but semantic drawing operations should be preferred when one already exists.

```cpp
auto controls = native::theme::create(event.g);
const native::theme::metrics metrics = controls->defaults();

native::theme::state button_state;
button_state.hot = pointer_inside;
button_state.pressed = mouse_down;
button_state.disabled = !can_continue;

controls->draw_button(native::rect(16, 16, 120, 32),
                      "Continue",
                      button_state);

controls->draw_menu_bar(native::rect(0, 56, 320,
                                     metrics.menu_bar_height));
controls->draw_menu_title(native::rect(4, 56, 52,
                                       metrics.menu_bar_height),
                          "File");

button_state.selected = true;
controls->draw_check(native::rect(160, 16, 140,
                                  metrics.check_height),
                     "Remember",
                     button_state);
```

Every theme operation preserves the context's ink, paper, pen, selected font,
and clip. See [Custom And Themed Drawing](PATTERNS-THEME.md) for backend
implementation rules and native-painter fallback behavior.

## Rendering themed controls into an image

Because an image supplies an ordinary `gpx`, it can also be used to render
control previews, documentation assets, or cached custom surfaces:

```cpp
native::img preview(260, 96);
native::gpx &g = preview.get_gpx();
g.clear(native::rgba(245, 245, 245, 255));

auto controls = native::theme::create(g);
controls->draw_button(native::rect(16, 16, 104, 30), "Apply");

native::theme::state selected;
selected.selected = true;
controls->draw_list_item(native::rect(16, 56, 180, 22),
                         "Selected item",
                         selected);

preview.save("control-preview.png");
```

This renders the active backend's native look where its painter supports the
image target, or that backend's native-look emulation otherwise.

## Practical rules

- Set the clip deliberately before clearing or drawing into a borrowed context.
- Keep paint-event contexts inside the callback and image contexts inside the
  image lifetime.
- Store application state, not window pixels, and repaint after invalidation.
- Measure with the selected font before positioning text or moving a caret.
- Use PNG for RGBA assets and JPEG for opaque photographic output.
- Prefer real Native controls for interaction and theme primitives for
  custom-drawn control visuals.
