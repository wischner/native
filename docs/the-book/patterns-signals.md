# Patterns: signal and event dispatching

This chapter explores the signal-slot mechanism used in the **native** UI library, enabling decoupled event handling between components. This pattern facilitates communication between objects without requiring them to be aware of each other's existence, promoting a modular and maintainable architecture.

---

## Signal-slot mechanism

The signal-slot system in **native** is inspired by the observer pattern. A **signal** represents an event, and **slots** are functions that respond to these events. When a signal is emitted, all connected slots are invoked.

This mechanism allows for:

- **Loose coupling**: Objects can communicate without tight dependencies.
- **Flexibility**: Multiple slots can respond to the same signal.
- **Type safety**: Ensures that signals and slots have matching signatures
- **Custom dispatch order**: Slots are called in reverse order of registration (most recently connected first)

---

## Implementation summary

The `signal` class is a template and supports arbitrary argument types. Here's a minimal sketch:

```cpp
template<typename... Args>
class signal
{
public:
    int connect(const std::function<bool(Args...)>& slot);
    void disconnect(int id);
    void disconnect_all();
    void emit(Args... args);
};
```

Internally, each signal stores its slots in a `std::map`, keyed by an auto-incremented connection ID.

The signal system also supports:

- Overloads for connecting to member functions:

  ```cpp
  int connect(SomeClass* obj, bool (SomeClass::*method)(Args...));
  int connect(const SomeClass* obj, bool (SomeClass::*method)(Args...) const);
  ```

- Optional **lazy initialization**, provided via a constructor that accepts an `initializer` function:

  ```cpp
  signal(std::function<void()> init); // Runs before first emit or connect
  ```

This allows toolkits to defer signal setup until the signal is actually used.

---

## Usage example

```cpp
signal<int> on_value_changed;

on_value_changed.connect([](int value) {
    std::cout << "Value changed to " << value << std::endl;
});

on_value_changed.emit(42);
```

Connected slots are executed until one of them returns `true`, at which point dispatch stops. This supports short-circuiting behavior.

---

## Signals in core UI classes

The signal system is built into the windowing model. For example, the `wnd` class declares:

```cpp
class wnd
{
public:
    signal<> on_wnd_create;
    signal<point> on_wnd_move;
    signal<size> on_wnd_resize;

    signal<point> on_mouse_move;
    signal<mouse_event> on_mouse_click;
    signal<mouse_wheel_event> on_mouse_wheel;
};
```

This provides hooks for toolkit backends to emit signals when native events are received, and for applications to connect custom behavior.

---

## Real-world example: X11 event dispatch

Here’s a concrete example of signals being emitted in an X11 main loop, based on low-level `XEvent` messages:

```cpp
while (running)
{
    XNextEvent(x11::cached_display, &event);

    native::wnd *wnd = x11::wnd_bindings.from_a(event.xany.window);
    if (!wnd)
        continue;

    switch (event.type)
    {
    case ConfigureNotify:
        wnd->on_wnd_resize.emit(size(event.xconfigure.width, event.xconfigure.height));
        wnd->on_wnd_move.emit(point(event.xconfigure.x, event.xconfigure.y));
        break;

    case MotionNotify:
        wnd->on_mouse_move.emit(point(event.xmotion.x, event.xmotion.y));
        break;

    case ButtonPress:
    case ButtonRelease:
    {
        mouse_button btn = mouse_button::none;

        switch (event.xbutton.button)
        {
        case Button1: btn = mouse_button::left; break;
        case Button2: btn = mouse_button::middle; break;
        case Button3: btn = mouse_button::right; break;
        case Button4:
        case Button5:
            mouse_wheel_event wheel(
                point(event.xbutton.x, event.xbutton.y),
                (event.xbutton.button == Button4 ? 1 : -1),
                wheel_direction::vertical);
            wnd->on_mouse_wheel.emit(wheel);
            break;
        }

        if (btn != mouse_button::none)
        {
            mouse_event e(btn, point(event.xbutton.x, event.xbutton.y));
            wnd->on_mouse_click.emit(e);
        }

        break;
    }

    case ClientMessage:
        // Handle WM_DELETE_WINDOW, etc.
        break;
    }
}
```

This dispatches native events to the appropriate signals in the `wnd` object, letting user-defined logic hook into mouse, move, and resize events easily.

---

## Example: connecting to a signal

A user application might handle window movement like this:

```cpp
app_wnd main_window("Signals example");

main_window.on_wnd_move.connect([](point p) {
    std::cout << "Window moved to: " << p.x << ", " << p.y << std::endl;
    return false; // Allow propagation
});
```

---

## Acknowledgment

The signal-slot implementation in **native** is based on [cpp-signal](https://github.com/Montellese/cpp-signal) by Montellese — a header-only, pure C++11 library providing signal and slot functionality.

It has been adapted to support lazy initialization, short-circuit logic, and member-function binding directly within the `native` library's architecture.
