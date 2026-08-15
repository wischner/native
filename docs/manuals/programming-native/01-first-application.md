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

// Start the application with a temporary main window.
int program(int, char **) {
    return native::app::run(native::app_wnd("Hello World!"));
}
```

`app::run()` performs four operations:

1. Detects the available screens.
2. Creates the native resource for the application window.
3. Shows the window.
4. Enters the backend event loop.

The temporary `app_wnd` remains alive for the complete call to `app::run()`.
For applications with controls or state, create a named window class as shown
in later chapters.

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

Next: [Painting and mouse input](02-painting-and-input.md).
