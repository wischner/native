# Patterns: Screens And Virtual Desktops

This chapter expands Section 9 of the architectural standards. Screen queries
use one process-owned snapshot so applications observe consistent display data
without accidental native queries.

## Detection and the snapshot

`screen::detect()` asks the selected backend for the active displays. A call
replaces the complete previous snapshot; it never appends to it and never
retains old results after a failed refresh.

The standard application startup calls `detect()` before it creates the main
window:

```text
app::run()
    |
    +-- refresh screen snapshot
    +-- create main window
    +-- show main window
    +-- enter event loop
```

Applications normally use the snapshot prepared by `app::run()`. They can call
`detect()` explicitly when they deliberately need to refresh the display
configuration, for example after a backend display-change notification.

## Cached queries

The following operations inspect only the current snapshot:

- `screen::count()` returns its number of screens.
- `screen::at(index)` returns the screen at a contiguous enumeration index.
- `screen::primary()` returns the one primary screen.
- `screen::virtual_bounds()` returns the union of all full display bounds.

None of these functions calls a native API or triggers detection. An empty
snapshot therefore produces a count of zero, null screen pointers, and empty
virtual bounds without trying to initialize a toolkit.

This distinction is intentional:

```cpp
native::screen::detect();       // Explicit native refresh.
int count = native::screen::count(); // Cached query only.
```

Code should never depend on a getter performing an implicit refresh.

## Screen data

Every detected screen contains:

- A contiguous zero-based index.
- Full bounds in virtual-desktop coordinates.
- A usable work area.
- A primary-screen flag.

`is_landscape()` is true when the full width is at least the full height, so a
square display is considered landscape.

Backends can enumerate outputs in sparse or unstable native orders. Shared
normalization rewrites public indices to `0` through `count() - 1`. The public
index describes the current snapshot only; it is not a permanent monitor ID.

## Primary screen

A non-empty snapshot contains exactly one primary screen. If a backend reports
more than one, normalization retains only the first. If it reports none,
normalization selects the first detected screen.

An empty snapshot has no primary screen, and `primary()` returns null.

Application code must handle that possibility even though a normal graphical
session usually has at least one display:

```cpp
native::screen *display = native::screen::primary();
if (!display)
    return false;
```

## Bounds and virtual coordinates

`bounds()` is the complete physical-display rectangle. It uses the same
virtual coordinate system as top-level windows. A display can begin at a
negative coordinate when it is positioned left of or above the primary
display.

`virtual_bounds()` forms the smallest rectangle containing every screen. It
must start with the first detected bounds rather than an assumed origin;
otherwise a desktop whose screens all begin at positive coordinates would be
incorrectly enlarged toward `(0, 0)`.

Overlapping or mirrored bounds remain valid. The result is a geometric union
rectangle, not a statement that every pixel inside it belongs to a display.
Gaps can exist in an irregular multi-monitor arrangement.

## Work areas

`work_area()` is the portion available for ordinary application windows after
excluding persistent system UI when the backend can obtain it. Examples
include taskbars, menu bars, docks, panels, and the Haiku Deskbar.

The work area is clipped to the screen's full bounds. If a toolkit cannot
provide a valid non-empty work area, Native uses the full bounds. Applications
can therefore rely on a usable rectangle for every detected screen without
learning each toolkit's fallback rules.

A main window can use the primary work area after screen detection:

```cpp
bool main_window::handle_create() {
    native::screen *display = native::screen::primary();
    if (!display)
        return false;

    const native::rect &available = display->work_area();
    set_position(available.p);
    return false;
}
```

Because `handle_create()` runs after startup detection, this does not trigger a
second refresh. The setter applies the cached position to the already-created
window.

## Snapshot lifetime

The vector returned by `detect()` belongs to the process. Pointers returned by
`at()` and `primary()`, references to `bounds()` or `work_area()`, and iterators
into the returned vector remain valid only until the next call to `detect()`.

Do not retain them across a refresh:

```cpp
native::screen *before = native::screen::primary();
native::screen::detect();
// before is no longer valid here; query primary() again.
```

Copy a `rect` or other value when it must survive detection. Re-query a screen
pointer after every refresh.

## No displays and detection failure

No active displays is a valid result. `detect()` installs an empty snapshot
and returns it.

A native API failure is different. The backend clears the old snapshot and
throws `std::runtime_error`. It must not log the failure and return stale or
partial results. This gives the caller an explicit choice about whether startup
should fail, retry, or report the problem.

If a failure occurs partway through enumeration, the backend releases all
temporary native resources before throwing. The visible snapshot remains
empty rather than containing an incomplete display list.

## Backend mappings

Every platform and toolkit implements the same public contract using its
native display facilities:

| Backend | Full bounds | Work area |
| --- | --- | --- |
| Windows | Monitor rectangle | Monitor work rectangle |
| Haiku | `BScreen` frame | Frame excluding a visible, non-auto-hidden Deskbar |
| macOS | `NSScreen` frame | `visibleFrame` |
| GEMix | Full VDI screen | AES desktop work rectangle |
| OpenMotif | X screen dimensions | EWMH `_NET_WORKAREA`, or full bounds |
| Window Maker | Active XRandR output bounds | EWMH `_NET_WORKAREA` clipped per output |
| SDL2 | Display bounds | SDL usable display bounds |
| X11 | Active XRandR output bounds | EWMH `_NET_WORKAREA` clipped per output |

Backend code must clear the old snapshot before querying, translate all
coordinates to Native types, add only active displays, release temporary
resources, and invoke shared normalization before returning.

## Responding to display changes

Detection is explicit, so a backend that supports live display-change events
should follow a deliberate sequence:

1. Receive the native configuration-change event.
2. Call `screen::detect()` at a safe point on the UI thread.
3. Discard all saved screen pointers and references.
4. Re-query the primary screen or relevant index.
5. Reposition or relayout windows only when application policy requires it.

The library does not hide this refresh inside `count()`, `primary()`, or
`virtual_bounds()`. That keeps native work visible and prevents an innocent
getter from invalidating pointers held by the caller.
