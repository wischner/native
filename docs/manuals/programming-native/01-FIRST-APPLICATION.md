# Chapter 1: Your First Application

The smallest native application creates an `app_wnd` and passes it to
`native::app::run()`.

## The application entry point

Portable native programs define `program()` instead of defining `main()` or
an operating-system-specific entry point. The selected backend initializes
`native::app::argc`, `native::app::argv`, and `native::app::envp`, then calls
your function.

The parameters can be unnamed when the application does not use command-line
arguments:

```cpp
//
// Demonstrates the smallest complete application built with native.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

// Start the application with a mutable main window.
int program(int, char **) {
    native::app_wnd window("Hello World!");
    return native::app::run(window);
}
```

`app::run()` performs four operations:

1. Detects the available screens.
2. Creates the native resource for the application window.
3. Shows the window.
4. Enters the backend event loop.

The non-`const` reference makes the lifecycle mutation explicit, and the named
`app_wnd` remains alive for the complete call to `app::run()`. For
applications with controls or state, create a named window class as shown in
later chapters.

## Window bounds

The default application window starts at `(100, 100)` with a client size of
`640` by `480` pixels. A title and explicit bounds can also be supplied:

```cpp
native::app_wnd window("My application", 80, 80, 800, 600);
return native::app::run(window);
```

Coordinates use `native::coord`; non-negative dimensions use `native::dim`.
The geometry chapter in the internal book explains the underlying value
types in more detail.

## Window icon

A top-level window can share an icon image. Decode it once, then pass the
shared pointer before or after the window is created:

```cpp
// img owns its pixels and is neither copied nor moved, so the
// decoded value initializes the shared image in place.
std::shared_ptr<const native::img> icon(
    new native::img(native::img::load("assets/app.png")));
native::app_wnd window("My application", 80, 80, 800, 600);
window.set_icon(icon);
return native::app::run(window);
```

Supply one image, ideally square and at least 64 pixels. Each backend fits a
copy into the square sizes it needs without distorting it. `set_icon(nullptr)`
restores the platform default, and `get_icon()` returns the shared image.

Where the icon appears depends on the desktop:

- Windows shows it in the title bar and taskbar.
- X11 window managers, CDE, OPEN LOOK and Window Maker show it in task lists
  and iconified windows.
- SDL2 passes it to its video driver.
- On macOS only the main window's icon is used, as the Dock image.
- GTK on Wayland, elementary OS and Haiku have no per-window icons. They keep
  the value but show the application's desktop-entry or executable icon
  instead.
- GEM windows have no icons either. On GEMix the main window's icon becomes
  the desktop's program icon when none is installed yet; see
  [Chapter 12](12-BUILDING-AND-DISTRIBUTING.md#program-icon).

Some desktops also write a label under an iconified window. It follows the
title unless you set a shorter one:

```cpp
native::app_wnd window("Vision - untitled document", 80, 80, 800, 600);
window.set_icon_text("Vision");
```

`get_icon_text()` returns the text you set, and an empty string makes the
label follow the title again. X11 window managers, CDE, OPEN LOOK and Window
Maker show it under the icon. OPEN LOOK icons are 64 pixels wide, so prefer
a short label there; macOS uses it for the minimized window in the
Dock. Windows, SDL2, GTK, Haiku and GEM keep the value but label minimized
windows with the title or file name.

[Chapter 10](10-GRAPHICS-IMAGES-FONTS-THEMES.md#png-and-jpeg-from-files-or-memory)
explains loading images from files or memory.

Next: [Painting and mouse input](02-PAINTING-AND-INPUT.md).
