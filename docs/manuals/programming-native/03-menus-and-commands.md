# Chapter 3: Menus and Commands

An `app_wnd` owns a `main_menu` model. Construct the model before the native
window is created so the backend can attach it during window creation.

## Building a menu

The stream-style interface alternates a top-level title with a
`menu_items()` group. A string creates an automatically numbered item. A
`std::pair<int, std::string>` assigns an explicit command identifier.

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
        menu << "File"
             << (native::menu_items("New")
                     << std::make_pair(
                            1, std::string("Open..."))
                     << std::make_pair(
                            2, std::string("Save"))
                     << std::make_pair(
                            99, std::string("Exit")))
             << "Edit"
             << (native::menu_items("Cut")
                     << std::string("Copy")
                     << std::string("Paste"))
             << "Help"
             << (native::menu_items("About...")
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

Next: [Buttons and control lifecycle](04-buttons.md).
