# Design patterns

This chapter documents the key design patterns used in the **native** library. These patterns are not arbitrary—they enable clean separation of concerns, portability across platforms, and consistency in how core logic interacts with native system resources.

We begin with the most foundational concept: how **pure C++ code interfaces with native handles**.

## Bridging pure C++ and native code

A core design goal of **native** is to maintain a clean, toolkit-agnostic C++ API exposed in `include/native.h`, without introducing any native resource types (such as `Widget`, `HWND`, `Window`, etc.) into the interface.

To achieve this, native system handles are stored and accessed using a **bi-directional bindings structure**, allowing native code to associate and retrieve portable C++ objects (like `wnd*`) with underlying OS or toolkit resources.

The pattern has three essential parts:

1. **Pure classes** in `native.h` do not contain native handles.
2. **Platform/toolkit-specific logic** lives in matching `*.cpp` files under `platforms/` and `toolkits/`.
3. A **bindings map** connects native handles with C++ class instances at runtime.

## Bindings map

The `bindings` class template lives in `src/bindings.h` and is hidden from public headers. It provides a two-way mapping between:

- A native handle type (e.g. `Widget`, `Window`, `SDL_Window*`)
- A native class pointer (e.g. `native::wnd*`, `native::app_wnd*`)

Example declaration in the SDL2 backend:

```cpp
namespace sdl
{
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
}
```

When a window is created, the toolkit code registers the pointer:

```cpp
sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
```

Later, it can retrieve either side of the mapping:

```cpp
SDL_Window *window = sdl::wnd_bindings.from_b(this);
native::wnd *wnd = sdl::wnd_bindings.from_a(window);
```

This system allows toolkit code to look up and interact with C++ class instances without requiring inheritance or native types in the public class declarations.

## Source file layering

The native library maintains a strict layering of source files:

- **Core logic** (pure C++) lives in `src/` and implements shared behavior (e.g. geometry, signals, window layout).
- **Platform code** (e.g. Linux, Windows) lives in `src/platforms/` and handles OS-level APIs.
- **Toolkit code** (e.g. X11, SDL2, Motif) lives in `src/toolkits/` and handles UI backends.

Each implementation reuses the same filename (e.g. `wnd.cpp`), split across layers:

```
src/wnd.cpp                     # Common pure logic
src/platforms/linux/wnd.cpp     # Linux-specific details
src/toolkits/sdl2/wnd.cpp       # SDL2-specific UI logic
```

This structure allows the linker to compose each part into a coherent whole, depending on the platform and toolkit selected during the build.

## Toolkit namespace isolation

Each toolkit or platform places all native helper logic inside a dedicated namespace. For example:

```cpp
namespace x11 { /* X11-specific helpers */ }
namespace motif { /* Motif-specific helpers */ }
namespace sdl { /* SDL2-specific helpers */ }
```

This avoids polluting the global namespace and keeps native integrations cleanly isolated from each other.

Toolkit code can still call into pure C++ methods (e.g. `create()`, `show()`), but implements the logic for those methods inside the appropriate `toolkits/` source files.

## Example: SDL2 main window

```cpp
void app_wnd::create() const
{
    SDL_Window *window = SDL_CreateWindow(
        title().c_str(), x(), y(), width(), height(),
        SDL_WINDOW_SHOWN);

    sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
}
```

Later, this same object can be retrieved and drawn to using the handle:

```cpp
SDL_Window *window = sdl::wnd_bindings.from_b(this);
SDL_ShowWindow(window);
```

This bridges native window management with pure C++ classes, without mixing concepts across layers.

---

More patterns will follow in this chapter, including:

- Geometry and type conventions
- Signal/event dispatching
- Graphics abstraction (`gpx`)
- Resource lifetime and ownership
