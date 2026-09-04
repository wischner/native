# Patterns: Clipboard And Text Editing

This chapter expands Architecture Sections 11 and 12. Clipboard data and text
editing share one important boundary: every backend converts its native
encoding and selection model into owned, validated UTF-8 before application
code sees it.

## Clipboard transactions

Open a read snapshot when data is needed, or an empty write transaction when
publishing data:

```cpp
native::clipboard output = native::clipboard::open_write();
output.write_text("first line\nsecond line")
      .write_image(preview)
      .commit();

native::clipboard input = native::clipboard::open_read();
if (input.has(native::clipboard_format::text))
    import_text(input.read_text());
if (input.has(native::clipboard_format::image))
    import_image(input.read_image());
```

A read object owns a consistent snapshot. A write object changes no system
state until `commit()`, and one commit may advertise both text and image
representations. Text is canonical UTF-8 with `\n` line endings. Images are
exposed as `img` and use lossless PNG bytes inside the portable stream.

Use `formats()`, `has()`, and `size()` to inspect a snapshot before reading.
The bounded `read()` and complete-byte `write()` operations support encoded
transfer without exposing a platform handle. The typed convenience functions
are preferred for normal application code.

The backend chooses the standard system service: Win32 clipboard, AppKit
pasteboard, Haiku `BClipboard`, X11 `CLIPBOARD`, or GEM AES scrap files. SDL's
text-only API is supplemented by a private X11 selection provider for PNG on
the X11 video backend. XView uses its Selection package for the standard X11
targets. Window Maker uses WINGs selection handlers for those same targets.
Native locks and borrowed pointers never escape these implementations.

GEM publishes lossless `SCRAP.PNG` together with the conventional monochrome
`SCRAP.IMG`. Native peers retain RGBA pixels while classic AES applications
can consume the standard bitmap scrap. Reads prefer PNG and fall back to IMG.

## Text editors are windows

`text_edit` is a child `wnd`, just like the other interactive controls. Choose
its mode at construction, assign a created parent, then create and show it:

```cpp
native::text_edit name("", native::text_edit_mode::single_line,
                       native::rect(12, 12, 220, 28));
name.set_parent(&window);
name.create();
name.show();
```

The mode is immutable because changing between native field and text-view
classes would otherwise replace the underlying window during its lifecycle.
Single-line values reject line breaks. Multiline values use portable line
feeds regardless of the native control's internal convention.

Windows, AppKit, Haiku, Athena, Motif, XView, and WINGs use their standard text
widgets. WINGs selects `WMTextField` or `WMText` according to mode. SDL2 and
GEMix emulate editing inside their existing backend event and paint paths,
including focus, selection, cursor movement, clipping, and scrolling.
Those details remain backend state; `get_text()` always returns the same
portable complete-value model. Emulated text selection uses the same themed
active/inactive selection background and text colors as lists and tables. SDL2
also reclamps horizontal scrolling after bounds changes, including controls
temporarily collapsed by a composite view.

## Live complete-value validation

A validator sees the proposed complete value, so it can enforce rules that
depend on more than the newly typed character:

```cpp
editor.set_validator([](const std::string &proposed) {
    return proposed.size() <= 64 &&
           proposed.find('\t') == std::string::npos;
});
```

The validator runs for typing, deletion, replacement, cut, and paste. Returning
false keeps the prior value and suppresses `on_change`. Installing a validator
also checks the current cached value immediately. Keep validators synchronous,
fast, and free of UI mutation because they execute inside native edit dispatch.

`set_text()` uses the same validation but is a programmatic property update and
does not emit. Accepted user edits update the cache and emit `on_change` once:

```cpp
editor.on_change.connect([](const std::string &complete_text) {
    update_model(complete_text);
    return false;
});
```

## Selection and clipboard commands

Applications can invoke `copy()`, `cut()`, `paste()`, and `select_all()`
directly. Ctrl+A/C/X/V, or the corresponding Command shortcuts on macOS, route
through the same functions. Therefore keyboard paste and direct paste have the
same validation and signal behavior.

`copy()` is available for read-only editors. `cut()` and `paste()` return false
when editing is disabled, no usable selection/format exists, or the proposed
replacement is rejected. Clipboard publication completes before a successful
cut removes text.
