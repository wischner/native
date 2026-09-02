# Patterns: Windows And App Windows

This chapter expands Section 5 of the architectural standards. `wnd` defines
the portable behavior shared by top-level windows and child controls. Backend
code supplies native resources without changing that behavior.

## Responsibilities of `wnd`

The base class owns or records only portable state:

- Cached bounds.
- A non-owning parent and non-owning child list.
- An owned layout manager.
- Access to a lazily created graphics abstraction.
- Lifecycle, geometry, paint, and input signals.
- Invalidation and lifecycle entry points.

Derived classes add properties and signals specific to one kind of window.
`app_wnd`, for example, adds a title, menu, and menu-command signal. `button`
adds its text and activation signal. `check` and `radio` add selection state,
while `list` adds an item model and single-selection signal. `text_edit` adds
single-line or multiline UTF-8 editing, read-only state, live validation, and
selection-aware clipboard commands.

`owned_wnd` adds a separate top-level owner relationship. Its public concrete
bases are `modeless_wnd` and `modal_wnd`; neither is a control or a layout
child.

These controls are real `wnd` subclasses. Backends use native widgets where
the platform supplies them: Athena Toggle/List/AsciiText, Motif
ToggleButton/List/Text, XView Panel controls, Win32 BUTTON/LISTBOX/EDIT, Haiku
BCheckBox/BRadioButton/BListView/BTextView, and AppKit
NSButton/NSTableView/NSTextField/NSTextView. Window Maker uses WINGs
command/switch/radio buttons, lists, text fields, and text views. SDL2 and
GEMix integrate equivalent controls into their toolkit-owned drawing and
input paths.

Native window handles, widgets, views, renderers, device contexts, and toolkit
callbacks never belong in these public classes. Backends keep them in private
bindings and graphics caches.

## Window lifecycle

A window moves through a small, consistent lifecycle:

```text
constructed -> created -> shown -> destroyed
                    ^                 |
                    +-----------------+
```

Construction records portable state. It must not create a native resource.
This lets application code configure properties, parents, layouts, and signals
before a backend is involved.

`create()` must be idempotent. On its first successful call it:

1. Verifies lifecycle prerequisites.
2. Creates the native resource.
3. Registers native bindings.
4. Applies every cached property.
5. Marks the object as created.
6. Emits `on_wnd_create` exactly once for that creation.

A repeated call while the resource exists does nothing. If a destroyed object
supports creation again, the next successful creation is a new lifecycle and
may emit one new create event.

`show()` requires a created resource. It exposes that existing resource; it
does not substitute for creation.

`destroy()` is also idempotent. It destroys child resources as required,
releases graphics resources, removes bindings, destroys the native resource,
and clears the created state. Destruction initiated by a toolkit must converge
on the same shared state through `on_native_destroy()`.

The same rule applies to a window-manager close command. A backend must not
leave the portable object marked as created after its native window has been
dismissed. Closing and then opening the same object is a fresh
`create()`/`show()` lifecycle; on OPEN LOOK both XView `ACTION_CLOSE` and
`ACTION_DISMISS` follow this path. OPEN LOOK owned windows are XView subframes
of their owner so dismissing one cannot be interpreted as quitting the root
application frame.

## Parent and child relationships

Parents and children refer to one another but do not own one another. The
application controls their C++ lifetimes. Destroying a C++ parent detaches
surviving child objects, and destroying a child removes it from the parent's
portable child list.

A child control needs an assigned, already-created parent before its own
native resource can be created. This ordering matters because most toolkits
require the native parent handle during child creation.

The main window's create signal is the standard place to create controls:

```cpp
class main_window final : public native::app_wnd
{
public:
    main_window()
        : native::app_wnd("Vision"),
          accept("Accept") {
        on_wnd_create.connect(this, &main_window::handle_create);
    }

private:
    native::button accept;

    bool handle_create() {
        accept.set_parent(this);
        accept.create();
        accept.show();
        return false;
    }
};
```

Keeping the button as a member makes its C++ lifetime cover the connection and
the main window's active event loop.

Parent assignment must reject hierarchy cycles. Reparenting a created object
must also preserve the backend's lifecycle requirements; an uncreated parent
cannot receive a created child.

## Owned top-level windows

Use an owned top-level window for a palette, inspector, auxiliary document
view, or dialog that remains associated with another application window but
needs independent screen geometry:

```cpp
class inspector_window final : public native::modeless_wnd
{
public:
    explicit inspector_window(native::app_wnd &owner)
        : native::modeless_wnd(
              owner, "Inspector", native::rect(720, 100, 280, 480)) {}
};
```

`get_owner()` reports the borrowed top-level owner. `get_parent()` remains
null, because the inspector is not clipped, positioned, or arranged as a child
control. Its bounds use the same virtual screen coordinates as `app_wnd`.

The application owns both C++ objects. The owner should normally outlive its
owned windows, but the portable owner graph safely detaches surviving objects
in either destruction order. Destroying the owner's native resource destroys
owned native resources first.

Backends express this relationship with their native concept: an owned Win32
top-level window, an Xt transient shell, an AppKit child window, a Haiku
floating subset, a WINGs top-level window tracked through the portable owner
graph, or an event-loop association on toolkits without native ownership.

`app_wnd::get_native_title_visible()` returns `true` by default. A specialized
compact tool window may override it before native creation and paint one
client caption instead. Docking floating panes use this hook on OPEN LOOK so
the draggable tool caption is not duplicated by an outer OLWM title.

## Modal dialogs

`modal_wnd` adds a result-bearing owner-modal session. It uses the same
construct, create, and show lifecycle as other windows:

```cpp
class confirm_window final : public native::modal_wnd
{
public:
    explicit confirm_window(native::app_wnd &owner)
        : native::modal_wnd(
              owner, "Confirm", native::rect(260, 180, 360, 160)) {}
};

confirm_window dialog(window);
dialog.on_modal_close.connect([](native::dialog_result result) {
    return result == native::dialog_result::accepted;
});
dialog.create();
dialog.show();
```

Showing the dialog starts modality and gives it focus. The application event
loop continues, so paint, resize, and destruction events remain live, but the
owner and its other owned branches cannot receive user input. Calling
`close(dialog_result::accepted)` or
`close(dialog_result::cancelled)` destroys the native dialog, restores the
previous eligible window, and emits `on_modal_close` exactly once. Closing it
through the window manager is cancellation.

Modal sessions stack. A second dialog should normally use the first dialog as
its owner; closing it restores the first dialog while the original owner stays
blocked.

File-open, file-save, folder, print, color, font, message, and similar system
dialogs are modal windows under this model. A backend may present an operating
system panel instead of a drawable Native window, but it must adapt that panel
to the same owner, modal-session, focus restoration, result, and close-signal
contract. Native dialogs do not get a separate nested portable event loop.

## File open and save dialogs

`open_file_dialog` and `save_file_dialog` specialize `file_dialog`, which in
turn specializes `modal_wnd`. They are logical system-panel objects: their
owner and result semantics are window semantics, but they have no paintable
client area and do not use ordinary window geometry hooks.

Configure the dialog, connect its completion signal, then create and show it:

```cpp
native::open_file_dialog dialog(window, "Open drawing");
dialog.set_initial_path("/home/user/Documents");
dialog.set_filters({
    {"Images", {"*.png", "*.jpg", "*.jpeg"}},
    {"All files", {"*"}}
});
dialog.set_allow_multiple(true);
dialog.on_modal_close.connect([&](native::dialog_result result) {
    if (result == native::dialog_result::accepted) {
        for (const std::string &path : dialog.get_paths())
            open_document(path);
    }
    return false;
});
dialog.create();
dialog.show();
```

`get_path()` returns the first selected path and is convenient for the common
single-file case. `get_paths()` preserves chooser order. A cancelled dialog
has no selected paths. Paths are UTF-8 strings; `file_filter` patterns use the
familiar forms such as `*.png` and `*.txt`.

A save dialog adds the filename-specific options:

```cpp
native::save_file_dialog dialog(window, "Export image");
dialog.set_initial_path("/home/user/Pictures");
dialog.set_suggested_name("drawing");
dialog.set_default_extension("png");
dialog.set_confirm_overwrite(true);
dialog.set_filters({{"PNG image", {"*.png"}}});
dialog.on_modal_close.connect([&](native::dialog_result result) {
    if (result == native::dialog_result::accepted)
        export_image(dialog.get_path());
    return false;
});
dialog.create();
dialog.show();
```

Use the close signal instead of assuming completion occurs before or after
`show()` returns. Windows, GEM, WINGs, and Linux desktop chooser processes are
synchronous; AppKit and Haiku panels and the Motif widget complete through
their native event dispatch. Both forms produce the same signal and result.

The backend selection follows native facilities: Windows Common Item Dialogs,
AppKit `NSOpenPanel`/`NSSavePanel`, Haiku `BFilePanel`, Motif
`XmFileSelectionBox`, XView `File_chooser`, WINGs
`WMOpenPanel`/`WMSavePanel`, and GEM AES `fsel_input`. Athena and SDL2 do not
include a standard chooser, so those Linux backends first invoke Zenity or
KDialog directly without a shell. If neither is installed, X11 presents a
browser made entirely from Athena widgets. SDL2 has no native widget set for
a compliant fallback, so it completes the session as cancelled and restores
its owner. Runtime chooser absence is not reported as an exception.

Some selectors expose fewer options. The Xaw fallback, Motif, XView, WINGs,
and GEM return one path even when multiple selection was requested. AppKit
and Haiku keep their standard overwrite safeguards even if confirmation was
disabled. These are conservative native degradations, not separate public
behavior.

## Layout ownership and geometry

A window owns its installed layout manager through a unique pointer. The
layout manager observes the window's non-owning child list and assigns child
bounds.

Portable geometry is always cached. Calling `set_position()`,
`set_dimensions()`, or `set_bounds()` updates that cache and applies it to a
created native resource. Dimension changes relayout children.

When the user or toolkit resizes a window, the backend calls
`on_native_resize()`. That updates cached dimensions and runs layout without
sending the same resize request back to the toolkit. Native move notifications
follow the same no-echo rule.

A notification that repeats the cached dimensions is ignored. Several backends
report geometry through a single event that also covers moves, so without this
a window would arrange its children every time it was dragged.

A layout pass never re-enters itself. A backend may report the size it granted
from inside the call that applied it, and geometry management in some toolkits
resizes a parent when a child changes size. Requests that arrive while a pass
runs are dropped, so applying geometry and arranging children is one pass
against the size the toolkit granted, rather than two against different ones.

A backend reports the client size its window really has once that window and
its menu exist. Where a menu bar or similar furniture sits above the client
area, the client is smaller than the window the toolkit created, and the first
arrangement has to use the smaller size to match what the backend renders.

## Invalidation and painting boundary

`invalidate()` asks the backend to schedule a repaint of all or part of the
client area. It does not paint immediately and does not emit `on_wnd_paint`
itself. The backend later receives a native paint event and performs the paint
flow described in [Window Painting](patterns-painting.md).

This separation allows native event coalescing and ensures drawing happens
with the correct native context and clip.

## Adding a window type

A new public `wnd` subclass is not complete when only its shared declaration
exists. Every supported backend must implement the same lifecycle and public
behavior.

The implementation checklist is:

1. Add portable properties, cached state, and public signals.
2. Implement shared validation and cache behavior.
3. Implement creation, cached-property application, showing, and destruction
   in every backend.
4. Add and remove native bindings at the correct lifecycle points.
5. Translate native events to public Native event types.
6. Support invalidation, painting, and graphics cleanup where applicable.
7. Add build coverage for every backend and runtime coverage where available.

Platform differences are expected below the public API. They must not produce
different public lifecycle or ownership rules.
