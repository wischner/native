# Patterns: application entry and main loop

This chapter explains the **application startup pattern** used in the **native** UI library. It defines how applications are structured, where execution begins, and how platform-specific entry points delegate to a portable `program()` function that drives the application lifecycle.

---

## Goals of the application pattern

The application pattern provides:

- A **standardized way to launch** native applications on any platform
- A **pure C++ entry point** (`program()`) for user code
- Platform-specific `main()` or `WinMain()` functions that handle low-level setup
- Support for **screen detection**, **window initialization**, and a **main loop**
- Future support for **per-frame updates** inside the loop (for animations or games)

---

## The `app` class

The `app` class coordinates application startup. It has no public constructor and is fully static:

```cpp
class app final
{
public:
    app() = delete;

    static int run(const app_wnd &wnd);
    static int main_loop();

    static app_wnd *main_wnd();

private:
    static inline app_wnd *_main_wnd = nullptr;
};
```

---

## Application flow

The standard application flow begins by calling:

```cpp
int program(int argc, char* argv[])
{
    return native::app::run(app_wnd("Hello World!"));
}
```

The `run()` method:

1. Detects connected screens
2. Creates and shows the main window
3. Stores the main window for access via `app::main_wnd()`
4. Enters the platform-specific main loop

---

## `app::run()` implementation

```cpp
int app::run(const app_wnd &wnd)
{
    screen::detect();

    wnd.create();
    _main_wnd = const_cast<app_wnd *>(&wnd);
    wnd.show();

    return app::main_loop();
}
```

This abstracts the entire initialization process behind a single call.

---

## Platform-specific entry point

Instead of writing a `main()` function yourself, the platform defines it for you and redirects to `program()`.

### Windows: `WinMain()`

```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // Parse command line into argc/argv
    // ...
    return program(argc, argv_c.data());
}
```

### Linux/macOS/Haiku: `main()`

```cpp
int main(int argc, char **argv)
{
    return program(argc, argv);
}
```

This ensures platform differences are abstracted away from your application logic.

---

## Main loop

Each platform implements its own message loop. For example, on Windows:

```cpp
int app::main_loop()
{
    MSG msg;
    BOOL ret;

    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (ret == -1)
            return -1;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
```

---

## Upcoming enhancement: per-frame updates

To support frame-based updates (e.g. for games, animations, or custom renderers), a **hook function** will be added to `main_loop()` and called between iterations. This allows applications to update internal state or redraw content without using timers.

This approach avoids breaking the message loop and maintains full compatibility with event-driven UIs.

---

## Summary

The application pattern in **native** gives you:

- A single entry point (`program()`)
- A self-contained `app::run()` startup flow
- Abstracted, per-platform main functions
- Consistent behavior across all supported platforms

It provides the foundation upon which all user-facing applications are built, while staying out of your way when not needed.
