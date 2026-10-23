# Chapter 15: Source Editing

Use `code_edit` for UTF-8 source that needs line numbers, syntax styles,
diagnostics, debugger marks, and completion. Use `text_edit` for ordinary
fields and unstyled multiline text.

## Build a source editor step by step

1. Keep a `code_edit` and a button as window members.
2. Configure a language and connect completion acceptance before creation.
3. Create both controls after the window; the button opens candidate choices.
4. Click Complete, use Up/Down and accept with Enter or Tab. Reopen and dismiss
   with Escape, or choose an item with the mouse.

The application supplies the inserted text. This example inserts at the caret;
replacing a partially typed prefix is the application's completion policy.

```cpp
//
// Opens source completion choices and inserts the accepted candidate.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>

class source_window : public native::app_wnd {
public:
    // Wire completion without introducing a language-server dependency.
    source_window() : app_wnd("Source", 100, 100, 680, 440) {
        _source.set_language("cpp").set_show_line_numbers(true);
        _complete.on_click.connect([this] {
            _source.show_completion({
                {"std::string", "std::string", "UTF-8 byte string"},
                {"std::vector", "std::vector", "Dynamic array"}});
            return true;
        });
        _source.on_complete.connect([this](native::completion_item item) {
            _source.insert(_source.get_caret_offset(), item.insert);
            return true;
        });
        on_wnd_create.connect([this] {
            attach(_source);
            attach(_complete);
            return true;
        });
    }
private:
    native::code_edit _source{"// Write here\n", {12, 60, 640, 350}};
    native::button _complete{"Complete", 12, 12, 160, 32};

    // The fixed bounds keep this introductory example independent of layouts.
    void attach(native::wnd &child) {
        child.set_parent(this);
        child.create();
        child.show();
    }
};

// Run the same source buffer on the selected backend.
int program(int, char **) {
    source_window window;
    return native::app::run(window);
}
```

## Create the editor

`code_edit` is a child window and follows the usual lifecycle:

```cpp
native::code_edit source("int main() {\n    return 0;\n}\n",
                         native::rect(12, 12, 640, 400));
source.set_language("cpp")
      .set_tab_width(4)
      .set_show_line_numbers(true);
source.set_parent(&window);
source.create();
source.show();
```

The buffer is canonical UTF-8 with `\n` endings. Line numbers are zero based
in the API, and source positions are UTF-8 byte offsets:

```cpp
source.go_to_line(2);
std::size_t start = source.line_start(2);
std::string line = source.line_text(2);
source.replace({start, start + line.size()}, "    return 1;");
```

Offsets must be on UTF-8 scalar boundaries. Insert, erase, replace, undo, and
redo emit `on_text_change`; programmatic property setters do not.

## Load and save plain source

```cpp
source.load("main.cpp");
if (source.get_load_warning())
    show_warning("Malformed UTF-8 was replaced with U+FFFD");

source.set_preserve_bom(true);
source.save();
```

Loading remembers LF, CRLF, or CR and normalizes the in-memory text. Saving
uses the remembered or explicitly selected `line_ending`. A loaded UTF-8 BOM
is restored only while `preserve_bom` is enabled. `set_path()`, `load(path)`,
`get_path()`, and `save_as(path)` use `std::filesystem::path`.

Markers, styles, diagnostics, caret, selection, and scrolling never enter the
source file. If your application restores those values from a session
sidecar, it owns and validates that JSON itself.

## Markers and diagnostics

The application decides what a gutter click means:

```cpp
source.on_gutter_click.connect([&source](int line) {
    const auto marks = source.markers();
    const bool set = std::any_of(
        marks.begin(), marks.end(), [line](const auto &mark) {
            return mark.line == line &&
                   mark.kind == native::marker_kind::breakpoint;
        });
    if (set)
        source.remove_marker(line, native::marker_kind::breakpoint);
    else
        source.add_marker(
            {line, native::marker_kind::breakpoint});
    return true;
});

source.set_diagnostics({
    {{18, 24}, native::diagnostic_severity::warning,
     "Unused value"}
});
```

Edits remap these overlays. A debugger can independently maintain a
`current_line` marker without placing metadata in the source.

## Syntax styles

When `code_theme::current_line` is unset, the active line uses the backend's
normal selected-row background and selected-text color. This keeps every
syntax fragment readable and visually consistent with native tables. Setting
`current_line` explicitly replaces only that background and retains the
configured syntax colors.

Implement `code_lexer` in application code and return sorted,
non-overlapping style runs:

```cpp
class cpp_lexer final : public native::code_lexer
{
public:
    std::string language_id() const override { return "cpp"; }

    std::vector<native::style_run> lex(
        std::string_view text,
        std::size_t dirty_start,
        std::size_t dirty_end) override {
        return find_cpp_styles(text, dirty_start, dirty_end);
    }
};

cpp_lexer lexer;
native::code_theme colors;
colors.styles.resize(3);
colors.styles[1].foreground = native::rgba(35, 70, 180, 255);
colors.styles[2].foreground = native::rgba(35, 125, 65, 255);
source.set_code_theme(colors).set_lexer(&lexer);
```

The editor borrows the lexer, so keep it alive until it is detached. A lexer
failure falls back to the default style and does not block editing.

## Completion

Provide filtered choices from your own index or language client:

```cpp
source.show_completion({
    {"std::string", "std::string", "UTF-8 byte string"},
    {"std::vector", "std::vector", "dynamic array"}
});
source.on_complete.connect(
    [&source](native::completion_item item) {
        source.insert(source.get_caret_offset(), item.insert);
        return true;
    });
```

Up and Down change the active choice, Enter or Tab accepts it, and Escape dismisses
the popup while keyboard focus remains in the editor. The shared painted
completion also accepts a mouse click on its visible row. SDL gives an open
completion priority over sibling text-field keyboard input. Language-server and
debugger protocols are application responsibilities.

Completion accepts the active choice with Enter or Tab. GTK transfers focus
to the completion keyboard host when the application opens the overlay.
The application still inserts the accepted item through `on_complete`.

Previous: [Advanced Table Views](14-ADVANCED-TABLE-VIEWS.md).
