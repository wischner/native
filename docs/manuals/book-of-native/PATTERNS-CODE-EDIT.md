# Patterns: Source Editing

This chapter expands Architecture Section 14. `code_edit` combines a portable
plain-text document with a source-oriented view; language tools remain in the
application.

## Document and file boundaries

The document is always well-formed UTF-8 with `\n` line endings. Public
offsets and `text_span` endpoints count UTF-8 bytes and must sit on scalar
boundaries. The line-start index makes line lookup independent of a backend's
native character or selection units. Tabs remain U+0009, with their displayed
width selected by the view.

Load and save form the only encoding boundary. Loading detects LF, CRLF, or
CR, strips a UTF-8 BOM, repairs malformed sequences with U+FFFD, and records
the detected choices. Saving expands canonical newlines and optionally
restores the loaded BOM. Presentation overlays are never serialized with the
source.

Insert, erase, and replace are document commands. Each retains a compact undo
record and remaps line markers, diagnostics, and style spans. `set_text()` is
a programmatic reset: it clears undo and overlays without emitting a user
change signal.

## Overlays and application services

Markers use zero-based lines. Diagnostics and syntax styles use half-open byte
spans. Style runs are sorted and non-overlapping, so rendering visible rows
does not require rich text in the source buffer.

An application may install a borrowed `code_lexer`. The editor passes the
canonical bytes and dirty range and accepts only valid style runs. No language
pack, parser, language server, or debugger is linked into the core library.
Likewise, completion items are supplied by the application. The control only
draws and navigates their popup, then reports the accepted value.

Gutter clicks report a line rather than changing a breakpoint. This keeps
debugger policy outside the control. An application may store paths, caret,
scroll position, language, and marks in a JSON sidecar, but that format is
application-owned and is never read or written by Native.

## Painting and backend adaptation

The gutter is library-painted everywhere because the supported native text
widgets do not share a portable gutter contract. The shared editor painter
uses the active backend's theme palette and semantic surfaces for its frame,
gutter, current row, selection, focus, diagnostics, markers, and completion
list. It does not impose one platform's visual design on another.

Backends own their host windows and translate focus, pointer, wheel, key, and
text-input events. The painted path keeps caret, selection, and vertical
scroll state in the portable editor while backend input services supply typed
text. Standard copy, cut, paste, select-all, undo, and redo shortcuts call the
same public operations used directly by applications.

See the application-facing
[Source Editing](../programming-native/15-SOURCE-EDITING.md) chapter for
construction, overlays, lexer integration, completion, and file handling.
