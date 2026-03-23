# Patterns: Application Entry And Main Loop

This chapter describes the application startup path that exists in the code
today.

## User entry point

Applications provide:

```cpp
int program(int argc, char **argv);
```

Backend-specific `main()` functions call `program()` and keep platform-specific
startup details out of user code.

Concept sample:

```cpp
int program(int argc, char **argv)
{
    return native::app::run(my_main_window());
}
```

## `app::run`

The shared startup path lives in `src/app.cpp`.

Today `app::run()` performs these steps:

1. detect screens
2. create the main application window
3. store the main window in `app`
4. show the main window
5. enter the backend main loop

That shared flow gives each backend the same application shape even though the
event loop implementation differs.

## Main window ownership

`app` stores a pointer to the main window so the backend can refer back to it
when needed.

This is part of the current architecture and is used by the Linux toolkit
implementations and the Windows backend during repaint and event dispatch.

## Backend main loops

The main loop itself is not implemented in the shared `src/` layer.
It is implemented by the selected backend.

That means:

- startup shape is shared
- event pumping is backend-specific
- native messages are translated into `native` events before they reach user code

In the current workflow, runtime checks exercise this model on Linux X11, Linux
SDL2, and Windows (via Wine).

## Screen detection

Screen detection happens before the main window is created.

The shared `screen` type stores:

- screen index
- bounds
- work area
- primary-screen flag

Backends populate that information in their own `screen::detect()` logic.

## Why this pattern exists

This keeps the public application model small:

- user code writes `program()`
- user code calls `app::run(...)`
- backend code owns platform startup and event processing

The result is a consistent entry model without exposing native startup details in
the public API.
