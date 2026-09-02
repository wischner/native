# Chapter 9: Owned Windows and File Dialogs

Ordinary child controls are clipped to their parent client rectangle and are
positioned by its layout. An owned top-level window is different: it has its
own screen rectangle and native top-level resource while remaining associated
with an application window.

Native exposes two concrete ownership policies:

- `modeless_wnd` leaves its owner and owned siblings interactive. Use it for
  inspectors, palettes, and auxiliary document views.
- `modal_wnd` takes focus and blocks input to its owner branch until it
  closes. Use it as the base for application dialogs.

Both classes derive from `owned_wnd`, which derives from `app_wnd`. Their
`get_owner()` pointer is non-owning. `get_parent()` remains null because an
owned top-level window is not a child control and does not enter the owner's
layout.

## A modeless owned window

Derive a reusable window in the same way as an application window:

```cpp
class inspector_window final : public native::modeless_wnd
{
public:
    explicit inspector_window(native::app_wnd &owner)
        : native::modeless_wnd(
              owner,
              "Inspector",
              native::rect(720, 100, 280, 480)) {}
};
```

Keep the inspector object alive, then create and show it after its owner has
been created:

```cpp
inspector_window inspector(window);
window.create();
window.show();
inspector.create();
inspector.show();
```

Call `center_to_parent()` before or after native creation to center an owned
window over its owner. The method updates the cached screen position and, for
an already-created window, moves the native window immediately:

```cpp
inspector.center_to_parent();
```

Calling it on a main window with no owner is a no-op. Centering follows the
top-level owner relationship, not child-control parenting.

`app::run()` normally creates and shows the main window, so application code
usually creates owned windows from the owner's `on_wnd_create` signal or from
a later command handler.

## A modal window

`modal_wnd::show()` begins an owner-modal session. The application event loop
continues to dispatch painting and system events, but blocked windows cannot
receive user input. Native does not expose a nested modal event loop.

Close a dialog with one final result:

```cpp
dialog.close(native::dialog_result::accepted);
dialog.close(native::dialog_result::cancelled);
```

Only one of these calls belongs to a session. Closing through the window
manager or destroying an unfinished dialog produces `cancelled`.
`on_modal_close` is emitted exactly once and `get_result()` retains the final
result. Closing also destroys the dialog's native resource, so call
`create()` again before reusing the C++ object for another session.

```cpp
class confirm_window final : public native::modal_wnd
{
public:
    explicit confirm_window(native::app_wnd &owner)
        : native::modal_wnd(
              owner,
              "Confirm",
              native::rect(260, 180, 360, 160)) {}
};

confirm_window dialog(window);
dialog.on_modal_close.connect(
    [](native::dialog_result result) {
        return result == native::dialog_result::accepted;
    });
dialog.create();
dialog.show();
```

Several dialogs owned by the same `app_wnd` form a modal stack. The newest
shown dialog receives input. Closing it restores the previous dialog; closing
the last one restores the owner and its modeless windows.

## Standard file dialogs

`open_file_dialog`, `save_file_dialog`, and `directory_dialog` apply the
modal-window contract to the standard chooser supplied by each platform or
toolkit. They are logical windows: they have an owner, lifecycle, result, and
completion signal, but no paintable client rectangle.

Keep a chooser as a member rather than a local variable. Some backends finish
from a later native event, while others finish before `show()` returns.

```cpp
//
// Demonstrates reusable standard open and save file dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>

#include <native.h>

class file_window : public native::app_wnd
{
public:
    file_window()
        : native::app_wnd(
              "File Dialogs", 100, 100, 460, 220),
          _open_button("Open...", 20, 20, 100, 32),
          _save_button("Save...", 140, 20, 100, 32),
          _open(*this, "Open image"),
          _save(*this, "Save image") {
        _open.set_filters({
            {"Images", {"*.png", "*.jpg", "*.jpeg"}},
            {"All files", {"*"}}
        });
        _open.set_allow_multiple(true);

        _save.set_filters({{"PNG image", {"*.png"}}});
        _save.set_suggested_name("drawing")
             .set_default_extension("png")
             .set_confirm_overwrite(true);

        on_wnd_create.connect(this, &file_window::on_create);
        on_wnd_paint.connect(this, &file_window::on_paint);
        _open_button.on_click.connect(
            this, &file_window::on_open);
        _save_button.on_click.connect(
            this, &file_window::on_save);
        _open.on_modal_close.connect(
            this, &file_window::on_open_close);
        _save.on_modal_close.connect(
            this, &file_window::on_save_close);
    }

private:
    native::button _open_button;
    native::button _save_button;
    native::open_file_dialog _open;
    native::save_file_dialog _save;
    std::string _status = "Choose a command.";

    void attach(native::wnd &control) {
        control.set_parent(this);
        control.create();
        control.show();
    }

    bool on_create() {
        attach(_open_button);
        attach(_save_button);
        return true;
    }

    bool on_open() {
        _open.create();
        _open.show();
        return true;
    }

    bool on_save() {
        _save.create();
        _save.show();
        return true;
    }

    bool on_open_close(native::dialog_result result) {
        _status = result == native::dialog_result::accepted
            ? "Open: " + _open.get_path()
            : "Open cancelled";
        invalidate();
        return true;
    }

    bool on_save_close(native::dialog_result result) {
        _status = result == native::dialog_result::accepted
            ? "Save: " + _save.get_path()
            : "Save cancelled";
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event event) {
        event.g.draw_text(_status, native::point(20, 90));
        return true;
    }
};

int program(int, char **) {
    file_window window;
    return native::app::run(window);
}
```

`get_path()` returns the first accepted path. `get_paths()` returns every path
in chooser order. `set_initial_path()` sets the starting location;
`file_filter` groups provide display names and wildcard patterns. Paths and
labels are UTF-8.

The open chooser can request multiple selection. OpenMotif, XView, WINGs,
and GEM selectors still return one path because that is what their standard
selector provides.
The save chooser can suggest a name, append a default extension, and request
overwrite confirmation. Platforms that always protect existing files retain
their native safeguard.

## Chooser used by each backend

| Backend | Standard chooser |
| --- | --- |
| Windows | Common Item Dialog |
| macOS | `NSOpenPanel` and `NSSavePanel` |
| Haiku | `BFilePanel` |
| OpenMotif | `XmFileSelectionBox` |
| OPEN LOOK | XView `File_chooser` |
| Window Maker | WINGs `WMOpenPanel` and `WMSavePanel` |
| GEMix | AES `fsel_input` |
| X11/Athena | Zenity, KDialog, then an Athena-widget browser |
| SDL2 | Zenity or KDialog |

Athena and SDL2 have no standard file chooser of their own. X11 can always use
its fallback browser because that browser is composed entirely from Athena
widgets. SDL2 has no native control set from which to compose such a browser,
so it delegates to an installed desktop chooser. If neither helper is
available, the SDL2 dialog completes as cancelled, restores its owner, and
does not throw merely because that runtime capability is absent.

The XView chooser is an owner-modal native OPEN LOOK command frame. It keeps
directories visible while applying filename filters, performs directory
navigation inside the chooser, and uses the same `dialog_result` completion
contract as every other backend.

The WINGs chooser is the toolkit's standard owner-modal browser panel. It
navigates directories and edits the selected leaf using native WINGs widgets.
WINGs 0.96 returns one path, so a multiple-selection request degrades to a
single accepted path.

`directory_dialog` uses the corresponding folder-selection mode of each
standard chooser. Its `set_allow_multiple()` request is preserved where the
native panel supports it and conservatively returns one folder on older
single-path selectors.

Next: [Graphics, images, fonts, and themes](10-GRAPHICS-IMAGES-FONTS-THEMES.md).
