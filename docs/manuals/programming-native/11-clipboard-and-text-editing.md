# Chapter 11: Clipboard and Text Editing

Clipboard data and text controls share one portable encoding boundary. Text is
valid, null-free UTF-8 with `\n` line endings. Images are represented by
`img`, with lossless PNG used for encoded transfer inside the portable
clipboard interface.

## Clipboard read snapshots

Open one snapshot and inspect its available representations before reading:

```cpp
native::clipboard input = native::clipboard::open_read();

if (input.has(native::clipboard_format::text)) {
    const std::string text = input.read_text();
    import_text(text);
}

if (input.has(native::clipboard_format::image)) {
    native::img image = input.read_image();
    import_image(image);
}
```

The snapshot owns its portable data, so later system clipboard changes do not
alter it. `formats()`, `has()`, and `size()` support discovery. The bounded
`read(format, offset, destination, capacity)` function supports encoded byte
transfer without exposing a native clipboard handle.

## Atomic clipboard writes

Opening a write transaction does not clear or change the system clipboard.
Stage one or more formats and publish them together with `commit()`:

```cpp
native::clipboard output = native::clipboard::open_write();
output.write_text("first line\nsecond line")
      .write_image(preview)
      .commit();
```

`write_text()` accepts portable UTF-8. `write_image()` stages a lossless image
representation. The general `write()` accepts complete encoded bytes and
validates them. A transaction can be committed once and must contain at least
one format. `get_committed()` reports whether publication succeeded.

The backend publishes standard system formats where available: Win32
clipboard formats, AppKit pasteboard types, Haiku `BClipboard` MIME data, X11
`CLIPBOARD` targets, and GEM scrap files. SDL2 uses its clipboard service for
text and an X11 selection provider for PNG when its active video driver is
X11. The OPEN LOOK backend publishes the same UTF-8 and PNG targets through
the XView Selection package. The Window Maker backend publishes them through
WINGs selection handlers. On other SDL video drivers, image clipboard
fallback is process-local.

## Single-line and multiline editors

`text_edit` is a child `wnd`. Its mode is fixed at construction because the
backend may need a different native widget for each mode:

```cpp
native::text_edit name(
    "",
    native::text_edit_mode::single_line,
    native::rect(12, 12, 220, 28));

native::text_edit notes(
    "One line\nAnother line",
    native::text_edit_mode::multi_line,
    native::rect(12, 52, 320, 140));
```

A single-line editor rejects line feeds. Both modes reject malformed UTF-8,
embedded nulls, and carriage returns. `get_text()` always returns the complete
portable value. `set_text()` validates and updates the property without
emitting a user-action signal. `set_read_only()` prevents changes while still
allowing selection and copy.

## Live complete-value validation

A validator receives the complete value proposed by typing, deletion,
replacement, cut, or paste:

```cpp
editor.set_validator([](const std::string &proposed) {
    return proposed.size() <= 12 &&
           std::all_of(
               proposed.begin(),
               proposed.end(),
               [](char character) {
                   return character >= '0' && character <= '9';
               });
});
```

Returning `false` leaves the previous value in both the C++ object and native
control and suppresses `on_change`. Installing a validator checks the current
value immediately; `set_text()` throws when the new value is rejected.
`clear_validator()` removes the application rule but retains UTF-8 and mode
validation.

Keep validators fast, synchronous, and free of UI mutation because they run
inside native edit dispatch.

## Direct commands and keyboard shortcuts

The control provides `copy()`, `cut()`, `paste()`, and `select_all()` so menus
and toolbar commands do not need backend code. Ctrl+A/C/X/V invoke the same
operations. On macOS, the corresponding Command shortcuts are used.

`copy()` also works in a read-only editor. `cut()` and `paste()` return false
when the control is read-only, the required selection or format is absent, or
validation rejects the replacement. Accepted typing and clipboard changes
update the cached complete value and emit `on_change(std::string)` once.

## A complete editing example

```cpp
//
// Demonstrates live validation and direct clipboard commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <string>

#include <native.h>

class editor_window : public native::app_wnd
{
public:
    editor_window()
        : native::app_wnd(
              "Text Editing", 100, 100, 520, 330),
          _number(
              "",
              native::text_edit_mode::single_line,
              native::rect(20, 20, 220, 28)),
          _notes(
              "Paste a copied number here.",
              native::text_edit_mode::multi_line,
              native::rect(20, 70, 460, 150)),
          _copy("Copy number", 20, 240, 120, 32),
          _paste("Paste into notes", 160, 240, 150, 32) {
        _number.set_validator(
            [](const std::string &proposed) {
                return proposed.size() <= 12 &&
                       std::all_of(
                           proposed.begin(),
                           proposed.end(),
                           [](char character) {
                               return character >= '0' &&
                                      character <= '9';
                           });
            });

        on_wnd_create.connect(this, &editor_window::on_create);
        on_wnd_paint.connect(this, &editor_window::on_paint);
        _number.on_change.connect(
            this, &editor_window::on_number_change);
        _copy.on_click.connect(this, &editor_window::on_copy);
        _paste.on_click.connect(this, &editor_window::on_paste);
    }

private:
    native::text_edit _number;
    native::text_edit _notes;
    native::button _copy;
    native::button _paste;
    std::string _status = "Enter at most 12 digits.";

    void attach(native::wnd &control) {
        control.set_parent(this);
        control.create();
        control.show();
    }

    bool on_create() {
        attach(_number);
        attach(_notes);
        attach(_copy);
        attach(_paste);
        return true;
    }

    bool on_number_change(std::string text) {
        _status = "Accepted digits: " + text;
        invalidate();
        return true;
    }

    bool on_copy() {
        _number.select_all();
        _status = _number.copy()
            ? "Number copied"
            : "Nothing to copy";
        invalidate();
        return true;
    }

    bool on_paste() {
        _notes.select_all();
        _status = _notes.paste()
            ? "Clipboard text pasted"
            : "No text available";
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event event) {
        event.g.draw_text(_status, native::point(20, 292));
        return true;
    }
};

int program(int, char **) {
    editor_window window;
    return native::app::run(window);
}
```

Windows, macOS, Haiku, Athena, OpenMotif, XView, and WINGs use their standard
text widgets. XView selects `PANEL_TEXT` or `PANEL_MULTILINE_TEXT`; WINGs
selects `WMTextField` or `WMText`. SDL2 and GEMix
keep cursor, selection, focus, scrolling, and painting in their backend
implementations while presenting the same public control behavior.

The XView startup resources bind copy, cut, and paste to Ctrl+C, Ctrl+X, and
Ctrl+V in addition to the historical OPEN LOOK function keys. The backend
handles Ctrl+A before the native editor action and applies the same validated
selection replacement used by the direct functions.

Next: [Building, linking, and distributing](12-building-and-distributing.md).
