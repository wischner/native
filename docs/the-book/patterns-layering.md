# Patterns: Source layering and native bindings

This chapter describes one of the most fundamental architectural patterns in the **native** UI library: how platform-specific behavior is isolated from pure C++ logic through careful source layering and a bi-directional binding mechanism.

This separation of responsibilities ensures that platform-dependent code never leaks into public headers, and allows the core API to remain clean, modern, and portable.

---

## Pure interface, native backend

At the heart of `native` is a commitment to a clean interface: the public API defined in `include/native.h` contains only modern, portable C++ constructs. No `HWND`, `Widget`, `Window`, or platform-specific headers are ever exposed.

Instead, platform or toolkit specifics are implemented behind the scenes — where they belong.

---

## The bindings map

To connect C++ objects with native handles (and vice versa), the library uses a `bindings<A, B>` structure — a bi-directional map between a native resource (e.g., a window handle) and the owning C++ class (e.g., `native::wnd*`).

This mechanism is defined in `src/bindings.h` and is internal to the implementation:

```cpp
namespace native
{
    template <typename A, typename B>
    class bindings
    {
    public:
        void register_pair(const A &a, const B &b);
        void unregister_by_a(const A &a);
        void unregister_by_b(const B &b);
        B from_a(const A &a) const;
        A from_b(const B &b) const;

    private:
        std::unordered_map<A, B> a_to_b_;
        std::unordered_map<B, A> b_to_a_;
    };
}
```

Each platform or toolkit backend defines its own instance of this mapping. For example, the SDL2 backend defines:

```cpp
namespace sdl
{
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
}
```

When a window is created, the object registers itself:

```cpp
sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
```

Later, toolkit code can easily map back and forth:

```cpp
SDL_Window *handle = sdl::wnd_bindings.from_b(this);
native::wnd *wnd = sdl::wnd_bindings.from_a(handle);
```

This allows for complete decoupling between native handles and cross-platform logic.

---

## Source file layering

The native library organizes its source code in three clear layers:

- **Pure cross-platform logic**  
  Lives in `src/` and implements shared behavior: geometry types, event signals, window base classes.

- **Platform-specific implementation**  
  Lives in `src/platforms/`, with one subdirectory per OS (e.g., `windows/`, `linux/`). This layer interfaces directly with the OS APIs.

- **Toolkit-specific UI code**  
  Lives in `src/toolkits/`, with one subdirectory per UI backend (e.g., `sdl2/`, `x11/`, `openmotif/`). This handles visual representation, drawing, event integration.

Each layer implements matching `.cpp` files — such as `wnd.cpp` — to keep code intuitive and consistent:

```text
src/wnd.cpp                       # Pure C++ base window logic
src/platforms/windows/wnd.cpp     # Windows-specific native window code
src/toolkits/sdl2/wnd.cpp         # SDL2 window drawing and events
```

This makes it easy to follow the control flow from abstract API to platform integration without confusing cross-pollution.

---

## Toolkit namespace isolation

All toolkit-specific code is enclosed in a dedicated namespace. This avoids polluting the global scope and ensures clean encapsulation:

```cpp
namespace x11    { /* X11-specific helpers */ }
namespace motif  { /* Motif-specific helpers */ }
namespace sdl    { /* SDL2-specific helpers */ }
```

For example, an SDL2 window might register and show itself like this:

```cpp
void app_wnd::create() const
{
    SDL_Window *window = SDL_CreateWindow(
        title().c_str(), x(), y(), width(), height(),
        SDL_WINDOW_SHOWN);

    sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
}

void app_wnd::show() const
{
    SDL_Window *window = sdl::wnd_bindings.from_b(this);
    SDL_ShowWindow(window);
}
```

This bridges native UI elements to their portable C++ counterparts — without ever exposing `SDL_Window*` in the public API.

---

This pattern — of strict source layering and bi-directional binding — is the cornerstone of how `native` achieves portability while preserving native integration.
