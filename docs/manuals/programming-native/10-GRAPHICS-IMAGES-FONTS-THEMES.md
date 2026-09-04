# Chapter 10: Graphics, Images, Fonts, and Themes

Native exposes one stateful `gpx` drawing interface for window paint events
and memory images. Application code does not construct a platform graphics
context. A paint event borrows a window context, while `img::get_gpx()` lazily
creates a context for the image's owned RGBA pixels.

## Drawing contexts

The common operations are:

| Operation | Purpose |
| --- | --- |
| `set_ink()` and `set_paper()` | Select foreground and background colors |
| `set_pen()` | Select line thickness in pixels |
| `set_font()` | Borrow a font for text and measurement |
| `set_clip()` and `get_clip()` | Limit and inspect the drawing rectangle |
| `clear()` | Fill the current clip |
| `draw_line()` | Draw one line segment |
| `draw_rect()` | Draw an outline or filled rectangle |
| `draw_ellipse()` | Draw a portable outlined or filled ellipse |
| `draw_polyline()` / `draw_polygon()` | Draw connected and closed point geometry |
| `draw_text()` | Draw UTF-8 text from a top-left position |
| `draw_img()` | Draw, crop, or scale an image |
| `save_state()` | Restore colors, pen, font, and clip at scope exit |

Calls return the context where practical, so related operations can be
chained:

```cpp
event.g.set_clip(event.r)
       .clear(native::rgba(255, 255, 255, 255))
       .set_ink(native::rgba(40, 80, 160, 255))
       .set_pen(2)
       .draw_rect(native::rect(12, 12, 180, 80))
       .draw_line(native::point(12, 52),
                  native::point(192, 52));
```

The invalid rectangle is `wnd_paint_event::r`. Never store
the event's borrowed context after the paint handler returns.

## Creating and drawing into images

`img` owns non-zero-width, non-zero-height RGBA pixels:

```cpp
native::img canvas(320, 200);
native::gpx &g = canvas.get_gpx();

g.clear(native::rgba(255, 255, 255, 0))
 .set_ink(native::rgba(20, 70, 140, 255))
 .draw_rect(native::rect(16, 16, 120, 60), true)
 .draw_text("Preview", native::point(24, 34));
```

`pixels()` provides contiguous mutable pixels when direct access is needed.
The image and its graphics context have the same lifetime; `img` is not
copyable.

Draw an image into either kind of context with the same call. Rectangle
overloads scale a complete image or a cropped source rectangle; choose linear
filtering for ordinary artwork and nearest filtering for pixel art:

```cpp
event.g.draw_img(canvas, native::point(40, 80));
event.g.draw_img(canvas,
                 native::rect(40, 80, 160, 100),
                 native::image_filter::linear);
event.g.draw_img(canvas,
                 native::rect(16, 16, 64, 64),
                 native::rect(220, 80, 96, 96),
                 native::image_filter::nearest);
```

Scaling preserves source alpha. The crop must remain inside the source image.

## Scoped state and bounded text

Use `save_state()` before temporary paint changes. Its move-only RAII object
restores ink, paper, pen thickness, selected font, and clipping even when the
scope exits early:

```cpp
{
    auto saved = event.g.save_state();
    event.g.set_clip(native::rect(20, 20, 180, 36))
        .set_ink(native::rgba(32, 64, 128, 255))
        .draw_text(
            "A long item label",
            native::rect(20, 20, 180, 36),
            {native::text_align::center,
             native::text_valign::center,
             native::text_overflow::ellipsis,
             true});
}
```

Bounded text uses logical start/center/end alignment, vertical alignment,
clipping, and optional end ellipsis. This remains plain text; rich spans and
editable layout belong in controls.

## PNG and JPEG from files or memory

The decoder identifies PNG and JPEG from their encoded contents, not from a
filename extension:

```cpp
native::img logo = native::img::load("assets/logo.png");
native::img received = native::img::decode(
    encoded.data(), encoded.size());
```

Decoded pixels are always RGBA. PNG retains alpha; JPEG produces opaque
pixels. Empty or malformed input and unsupported data produce a standard
exception. File-loading and saving paths use `std::filesystem::path`.

Encoding can return a memory vector or write a file:

```cpp
std::vector<std::uint8_t> png =
    canvas.encode(native::image_format::png);
std::vector<std::uint8_t> jpeg =
    canvas.encode(native::image_format::jpeg, 92);

canvas.save("preview.png");
canvas.save("preview.jpg", 92);
```

JPEG quality is from 1 through 100 and defaults to 90. Use PNG whenever alpha
must survive a round trip. `save()` accepts `.png`, `.jpg`, and `.jpeg`
extensions without regard to case.

Linux backends use libpng and libjpeg. Windows uses GDI+, Haiku uses the
Translation Kit, and macOS uses ImageIO. Chapter 12 lists the corresponding
link and deployment requirements.

## Stock, installed, file, and memory fonts

Stock fonts express intent instead of naming a platform face:

```cpp
const native::font_t &body =
    native::font_t::stock(native::font_role::system);
const native::font_t &code =
    native::font_t::stock(native::font_role::fixed);
const native::font_t &caption =
    native::font_t::stock(native::font_role::small);
```

The other roles are `icon_label`, `title`, and `control`. Stock handles live
for the process and are borrowed by callers.

Enumerate installed faces when building a font chooser:

```cpp
std::vector<native::font_description> installed =
    native::font_t::enumerate_installed();

native::font_t selected;
if (!installed.empty()) {
    selected = native::font_t::from_file(
        installed.front().path,
        16,
        installed.front().face_index);
}
```

Each description reports the family, style, face name,
`std::filesystem::path`, weight,
italic and fixed-pitch flags, and face index. Enumeration describes the
current system; do not assume the same faces exist on another machine.

An application can create a face from a TrueType/OpenType file or from bytes
already in memory:

```cpp
native::font_t from_disk = native::font_t::from_file(
    "assets/Inter-Regular.ttf", 16);

native::font_t embedded = native::font_t::from_memory(
    font_bytes.data(), font_bytes.size(), 16, 0);
```

The memory factory copies the bytes before returning. Both factories return
an invalid, move-only handle for malformed data, an invalid size, or a missing
collection face. Check `valid()` before selecting it. Portable file and memory
fonts use the library's vendored TrueType rasterizer, so no extra font engine
must be distributed.

`gpx::set_font()` borrows its argument. The selected font must therefore
outlive every drawing and measurement call made through that context.

## Measuring text for editors and layouts

Measure with the same font that will draw. A `font_t` measures directly, and a
`gpx` measures through its currently selected font:

```cpp
const native::font_metrics face = code.get_metrics();
const native::text_metrics line = code.measure_text("return 0;");
const native::text_metrics glyph = code.measure_character(U'W');

g.set_font(code);
const native::text_metrics prefix = g.measure_text("return ");
const native::text_metrics cursor = g.measure_character(U'0');
```

`font_metrics` contains `ascent`, `descent`, `leading`, `height`, and
`max_advance`. `text_metrics` contains painted `width`, `height`, and cursor
`advance`. Character measurement accepts one Unicode code point. Text
measurement accepts UTF-8.

These values are the primitives needed for caret placement, selection bounds,
line spacing, hit testing, and scroll extents. A full editor must additionally
choose grapheme, shaping, bidirectional-text, wrapping, and tab policies.

## Native-look themed drawing

Use real `button`, `check`, `radio`, `list`, and `text_edit` windows for an
interactive interface. They supply lifecycle, focus, keyboard behavior, and
native accessibility. Use `theme` only when a custom-drawn surface needs a
familiar control visual.

When deriving one of those controls, override only the protected visual stage
you own. Simple controls dispatch background, border or indicator,
text/content, and focus in that order. `app_wnd` and `panel` expose a
background stage; `split_view` exposes splitter-background and grip stages.
Calling a stage's base implementation retains the complete default for that
stage. Native-widget backends honour the request where their toolkit provides
owner/custom drawing; otherwise they retain native rendering and behavior.

Create a short-lived theme around a borrowed context:

```cpp
auto controls = native::theme::create(event.g);

native::theme::state state;
state.hot = pointer_inside;
state.pressed = mouse_down;
state.disabled = !can_continue;

controls->draw_button(
    native::rect(16, 16, 120, controls->get_button_height()),
    "Continue",
    state);
controls->draw_menu_bar(
    native::rect(0, 60, 320, controls->get_menu_bar_height()));

state.selected = true;
controls->draw_check(
    native::rect(160, 16, 140, controls->get_check_height()),
    "Remember",
    state);
```

The semantic primitives draw buttons, menu bars, menu titles, menu items,
popup frames, list items, checks, radios, complete lists, and text-edit
frames. Advanced controls compose surfaces, row/tile selections, focus,
disclosure indicators, separators, and scrollbar parts. `state` describes
hot, pressed, selected, disabled, focused, and active visuals. `defaults()`
supplies backend metrics; `native_palette()` supplies colors for custom
compositions.

Use the panel surface/color for ordinary window and container background.
Reserve the content surface/color for white editing and item areas such as
text editors, lists, icon views, trees, and tables. This distinction lets
standard controls sit naturally on the surrounding window chrome.

Prefer the named metric and color getters in new code. There is a getter for
every field in `theme::metrics` and `theme::palette`, including button,
editor, menu, list, table, tree, header, tab, icon-view, scrollbar, status-bar,
ruler, selection, separator, and focus values. Color getters always return
opaque values, so callers do not need to recognize a transparent fallback
sentinel. Variable-length shapes also have orientation-aware helpers:

```cpp
const native::size scroll = controls->get_scrollbar_size(
    native::scrollbar_orientation::vertical, viewport_height);
const native::size ruler = controls->get_ruler_size(
    native::ruler_orientation::horizontal, viewport_width);

event.g.clear(controls->get_content_background_color())
       .set_ink(controls->get_content_foreground_color());
```

The backend uses its native painter where that painter supports the target:
Windows uses GDI control primitives, OpenMotif uses Motif shadow primitives,
OPEN LOOK uses OLGX with the active XView Panel color map and font resources,
Window Maker uses WINGs relief drawing, screen colors and fonts, and native
indicator pixmaps, Haiku uses `BControlLook`, and macOS uses AppKit cells.
X11/Athena, SDL2, and GEMix keep backend-specific native-look emulation. Every backend also has a
fallback for targets, such as memory images, that its native painter cannot
draw into.

A theme only paints. The caller remains responsible for layout, hit testing,
focus, input state, and invalidation. Theme operations restore the context's
clip, colors, pen, and selected font before returning.

Next: [Clipboard and text editing](11-CLIPBOARD-AND-TEXT-EDITING.md).
