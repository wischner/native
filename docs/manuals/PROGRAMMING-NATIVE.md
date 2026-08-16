# Programming Native

This manual teaches application programming with the `native` C++ user
interface library. It starts with the smallest possible window and then
introduces painting, input events, menus, controls, layouts, independent
windows, system dialogs, graphics, fonts, images, clipboard access, and text
editing.

The first chapters preserve the former runnable examples as maintained
documentation while the repository itself builds only the `vision`
application. Later chapters cover the expanded API with complete programs,
focused snippets, and backend deployment instructions.

## How a native program is organized

A native application has three important pieces:

1. Include the public API with `#include <native.h>`.
2. Define an application window, usually by deriving from
   `native::app_wnd`.
3. Implement `program()`, which creates the window and passes it to
   `native::app::run()`.

The library supplies the operating-system entry point and event loop. Your
code supplies `program()` and reacts to signals emitted by windows and
controls.

## Table of contents

1. [Your first application](programming-native/01-first-application.md)
2. [Painting and mouse input](programming-native/02-painting-and-input.md)
3. [Menus and commands](programming-native/03-menus-and-commands.md)
4. [Buttons and control lifecycle](programming-native/04-buttons.md)
5. [Configuring controls](programming-native/05-configuring-controls.md)
6. [Absolute layout](programming-native/06-absolute-layout.md)
7. [Grid and nested layout](programming-native/07-grid-layout.md)
8. [Selection controls](programming-native/08-selection-controls.md)
9. [Owned windows and file dialogs](programming-native/09-owned-windows-and-dialogs.md)
10. [Graphics, images, fonts, and themes](programming-native/10-graphics-images-fonts-themes.md)
11. [Clipboard and text editing](programming-native/11-clipboard-and-text-editing.md)
12. [Building, linking, and distributing](programming-native/12-building-and-distributing.md)

## Building the repository program

Configure a debug build under the root `build/` directory and select a Linux
toolkit:

```bash
cmake -S . -B build/linux-x11 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTOOLKIT=X11
cmake --build build/linux-x11
```

The application is produced as `build/linux-x11/src/vision`. Other backends
use the same source program and place their result in the corresponding build
tree. Chapter 12 gives the exact configure command, link libraries, build
dependencies, and deployment requirements for every supported platform and
Linux toolkit.

## Conventions used in the chapters

- Windows and controls are ordinary C++ objects.
- Parent and child relationships are non-owning; the application retains its
  control objects.
- Signals call connected handlers. Returning `true` stops further signal
  propagation.
- State changes that affect painting call `invalidate()` to request a repaint.
- Paint handlers draw only through the `gpx` object carried by the paint
  event.
- Portable strings are UTF-8. Text editors normalize line endings to `\n`.
- Backend headers and handles never enter application code. Include
  `<native.h>` and link the CMake target `native`.

Continue with [Your first application](programming-native/01-first-application.md).
