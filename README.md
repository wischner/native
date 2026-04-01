![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

# Welcome to native

_by Tomaz Stih_

**native** is a minimal, cross-platform UI library targeting **Linux**, **Windows**, **Haiku**, and **macOS**. It is designed to serve as a lightweight abstraction layer over the native UI toolkits provided by each operating system, exposing a consistent and modern C++ interface.

The focus is on **clarity**, **minimalism**, and **practicality**:

- Minimal code to achieve common UI tasks.
- Consistent, lowercase API design inspired by the C++ standard library.
- No console application support—this is strictly for graphical user interfaces.

## Why another UI library?

**native** is not intended to compete with larger frameworks. Instead, it serves two main purposes:

1. It provides a clean, modern C++ API for simple and native UI development across three operating systems.
2. It is written in the open, **chapter by chapter**, allowing developers to understand exactly how each component works.  
   The development process is transparent, aiming to demystify cross-platform UI programming.

If you are looking for a straightforward, understandable UI library, or if you want to learn how to build one from scratch, **native** may be of interest.

## Features

- **Backend coverage today**: Linux (X11, SDL2, OpenMotif build path, GNUstep build path), Windows (WinAPI), Haiku (API), macOS (Cocoa code path)
- **Native controls**: Direct use of system-native widgets and event loops
- **Minimal and modern C++**: Clean code, few dependencies
- **Educational**: Open development process, detailed documentation in chapters
- **Consistent lowercase API**: Naming inspired by the C++ standard library

## Minimal working native app

```cpp
#include "native.h"

int program(int argc, char* argv[])
{
    return native::app::run(app_wnd("Hello World!"));
}
```

## Painter example

Demonstrates mouse input and custom painting. Subclass `app_wnd`, connect to the mouse and paint signals, and draw with the graphics context passed to the paint handler.

```cpp
#include <native.h>
#include <vector>

class painter_window : public native::app_wnd
{
public:
    painter_window()
        : native::app_wnd("Native Painter"), _drawing(false)
    {
        on_mouse_click.connect(this, &painter_window::on_click);
        on_mouse_move.connect(this, &painter_window::on_move);
        on_mouse_wheel.connect(this, &painter_window::on_wheel);
        on_wnd_paint.connect(this, &painter_window::on_paint);
    }

private:
    std::vector<std::vector<native::point>> _strokes;
    bool _drawing;

    bool on_click(native::mouse_event e)
    {
        if (e.button == native::mouse_button::left)
        {
            if (e.action == native::mouse_action::press)
            {
                _strokes.push_back({e.position});
                _drawing = true;
            }
            else if (e.action == native::mouse_action::release)
                _drawing = false;
            invalidate();
        }
        return true;
    }

    bool on_move(native::point p)
    {
        if (_drawing) { _strokes.back().push_back(p); invalidate(); }
        return true;
    }

    bool on_wheel(native::mouse_wheel_event)
    {
        _strokes.clear(); _drawing = false; invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        for (const auto &stroke : _strokes)
            for (size_t i = 1; i < stroke.size(); ++i)
                e.g.draw_line(stroke[i - 1], stroke[i]);
        return true;
    }
};

int program(int argc, char *argv[])
{
    return native::app::run(painter_window());
}
```

Hold the left mouse button and drag to draw freehand strokes. Scroll the mouse wheel to clear the canvas.

## Menu example

Demonstrates how to build a menu bar with submenus and respond to menu item selections. Menus are populated before the window is shown using the `<<` operator. Items can carry an integer ID for identification in the handler.

```cpp
#include <string>
#include <native.h>

class menu_window : public native::app_wnd
{
public:
    menu_window()
        : native::app_wnd("Menu Example", 100, 100, 640, 480)
    {
        menu << "File"
             << (native::menu_items("New")
                     << std::make_pair(1, std::string("Open..."))
                     << std::make_pair(2, std::string("Save"))
                     << std::make_pair(99, std::string("Exit")))
             << "Edit"
             << (native::menu_items("Cut")
                     << std::string("Copy")
                     << std::string("Paste"))
             << "Help"
             << (native::menu_items("About...")
                     << std::make_pair(100, std::string("License")));

        on_menu.connect(this, &menu_window::on_menu_item);
        on_wnd_paint.connect(this, &menu_window::on_paint);
    }

private:
    int _last_id = 0;

    bool on_menu_item(int id)
    {
        if (id == 99) { destroy(); return true; }
        _last_id = id;
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));
        std::string msg = _last_id > 0
            ? "Selected menu item ID: " + std::to_string(_last_id)
            : "Click a menu item above.";
        e.g.draw_text(msg, native::point(20, 60));
        return true;
    }
};

int program(int argc, char **argv)
{
    return native::app::run(menu_window());
}
```

Menu items without an explicit ID are label-only entries (the first item in each `menu_items` group). Items with an ID fire `on_menu` when selected. Selecting **Exit** (ID 99) calls `destroy()` to close the window and end the app.

## Button example

Demonstrates creating a button control, attaching it to the main window once the window exists, and reacting to clicks through `on_click`.

```cpp
#include <native.h>

class button_window : public native::app_wnd
{
public:
    button_window()
        : native::app_wnd("Button Example"),
          _button("Click me", 20, 20, 120, 32)
    {
        on_wnd_create.connect(this, &button_window::on_create);
        _button.on_click.connect(this, &button_window::on_button_click);
    }

private:
    native::button _button;
    int _click_count = 0;

    bool on_create()
    {
        _button.set_parent(this);
        _button.create();
        _button.show();
        return true;
    }

    bool on_button_click()
    {
        ++_click_count;
        invalidate();
        return true;
    }
};
```

## Building

Linux `X11`, `SDL2`, `OpenMotif`, and `GNUstep`, Windows `MinGW-w64`, and Haiku cross-builds can be driven through Docker from CMake so the required headers and tools come from known images instead of the host machine.

```bash
cmake -S . -B out
cmake --build out --target docker-x11
cmake --build out --target docker-sdl2
cmake --build out --target docker-openmotif
cmake --build out --target docker-gnustep
cmake --build out --target docker-win
cmake --build out --target docker-haiku
```

The Docker-backed targets build:

- `X11` into `build/linux-x11/`
- `SDL2` into `build/linux-sdl2/`
- `OpenMotif` into `build/linux-openmotif/`
- `GNUstep` into `build/linux-gnustep/`
- Windows MinGW-w64 into `build/windows-mingw-w64/`
- Haiku into `build/haiku/`

Current exercised runtime paths are:

- Linux X11
- Linux SDL2
- Windows MinGW binaries run through Wine
- Haiku binaries built locally through Docker, then copied to a Haiku machine and run there

Current additional build-verified path is:

- Linux OpenMotif through `docker-openmotif`
- Linux GNUstep through `docker-gnustep`

## The book of native

Explore the full, chapter-by-chapter explanation of how the **native** UI library is built.

[Read the book »](docs/the-book/index.md)

[language.url]: https://isocpp.org/
[language.badge]: https://img.shields.io/badge/language-C++-blue.svg
[standard.url]: https://en.wikipedia.org/wiki/C%2B%2B#Standardization
[standard.badge]: https://img.shields.io/badge/C%2B%2B-20-blue.svg
[license.url]: https://github.com/tstih/nice/blob/master/LICENSE
[license.badge]: https://img.shields.io/badge/license-MIT-blue.svg
[status.badge]: https://img.shields.io/badge/status-unstable-red.svg
