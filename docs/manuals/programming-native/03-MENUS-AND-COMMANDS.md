# Chapter 3: Menus and Commands

An `app_wnd` owns a `main_menu` model. Construct the model before the native
window is created so the backend can attach it during window creation.

## Building a menu

The stream-style interface alternates a top-level title with a
`menu_items()` group. A string creates an automatically numbered item. A
`std::pair<int, std::string>` assigns an explicit command identifier.
Insert `native::menu_separator` between related command groups. Separators
have no command identifier and cannot emit `on_menu`.

Place `&` before a character to select its keyboard mnemonic and write `&&`
for a literal ampersand. Append a tab and an accelerator name such as
`\tCtrl+O` when an item has a keyboard shortcut. The model removes that
markup from the visible label and gives each backend the mnemonic position
and shortcut separately. A label without an explicit `&` uses its first
character as a compatibility mnemonic.

Backends display the shortcut in the command row and install it as a keyboard
accelerator. Pressing it dispatches the same command ID as selecting the item.

The `on_menu` signal emits the identifier selected by the user. Commands that
matter to application logic should use explicit IDs.

```cpp
//
// Demonstrates menu construction and command dispatch with native.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>
#include <utility>

#include <native.h>

class menu_window : public native::app_wnd
{
public:
    // Construct the window, its menu model, and event connections.
    menu_window()
        : native::app_wnd(
              "Menu Example", 100, 100, 640, 480) {
        menu << "&File"
             << (native::menu_items("&New")
                     << std::make_pair(
                            1, std::string("&Open...\tCtrl+O"))
                     << std::make_pair(
                            2, std::string("&Save\tCtrl+S"))
                     << native::menu_separator
                     << std::make_pair(
                            99, std::string("E&xit\tAlt+F4")))
             << "&Edit"
             << (native::menu_items("Cu&t\tCtrl+X")
                     << std::string("&Copy\tCtrl+C")
                     << std::string("&Paste\tCtrl+V"))
             << "&Help"
             << (native::menu_items("&About...")
                     << std::make_pair(
                            100, std::string("License")));

        on_menu.connect(this, &menu_window::on_menu_item);
        on_wnd_paint.connect(this, &menu_window::on_paint);
    }

private:
    int _last_id = 0;

    // Execute a selected command and refresh displayed state.
    bool on_menu_item(int command_id) {
        if (command_id == 99) {
            destroy();
            return true;
        }

        _last_id = command_id;
        invalidate();
        return true;
    }

    // Display the most recently selected command identifier.
    bool on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        const std::string message = _last_id > 0
            ? "Selected menu item ID: " +
                  std::to_string(_last_id)
            : "Click a menu item above.";
        event.g.draw_text(message, native::point(20, 60));
        return true;
    }
};

// Create the menu window and enter the native event loop.
int program(int, char **) {
    menu_window window;
    return native::app::run(window);
}
```

## Closing an application

Calling `destroy()` on the main application window releases its native
resource. Backends use that destruction to leave the event loop when no main
window remains.

Menu commands named Open and Save do not imply a file-selector API. Connect
those commands to the standard `open_file_dialog` and `save_file_dialog`
objects described in Chapter 9.

The OPEN LOOK backend materializes this model as XView Panel menu buttons and
OpenMenu command menus, so menu interaction remains inside the XView notifier.
The Window Maker backend uses a click-persistent, context-menu-style popup:
one click opens it and it remains visible until a command, another menu, or an
outside click is chosen. It sizes to its labels, underlines mnemonics,
right-aligns accelerator names, and supports Alt plus a top-level mnemonic,
arrow navigation, item mnemonics, and registered accelerators. This matches
Window Maker application menus rather than the press-drag selection behavior
of a WINGs `WMPopUpButton`.

Next: [Buttons and control lifecycle](04-BUTTONS.md).
