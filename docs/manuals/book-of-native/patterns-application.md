# Patterns: Application Entry And Main Loop

This chapter expands Section 8 of the architectural standards. Native separates
the portable application entry point from the entry point and event loop
required by an operating system or toolkit.

## Why applications define `program()`

Not every target begins with a conventional C or C++ `main()`. Windows may use
`WinMain`, while another target can require a framework launcher, registration
object, or different argument representation.

Application code therefore defines one portable function:

```cpp
int program(int argc, char **argv) {
    main_window window;
    return native::app::run(window);
}
```

`program()` returns the process exit code but knows nothing about the native
launcher. It includes only the public Native API, creates the application's
main window object, and hands that object to `app::run()`.

Application code must not:

- Define `main()`, `WinMain`, or another platform entry point.
- Use native argument or application types.
- Start a toolkit event loop directly.
- Call a backend initialization function.
- Call `app::main_loop()` itself.

## Backend launcher

Each platform supplies the entry point expected by its environment. The
launcher has a narrow responsibility:

1. Receive native startup arguments.
2. Normalize them into `argc`, `argv`, and `envp` where the platform provides
   those concepts.
3. Store them in `native::app::argc`, `native::app::argv`, and
   `native::app::envp`.
4. Call `program()` exactly once.
5. Return the value from `program()` to the operating system.

The launcher does not construct application controls or reproduce the shared
startup sequence. Those actions belong to portable application code and
`app::run()`.

## Roles of the startup classes

`app` is a static coordinator. It cannot be instantiated or used as an
application base class. Its job is to manage the one active application run,
publish normalized arguments, expose the borrowed main window during that run,
and enter the selected backend loop.

`app_wnd` is the portable top-level application-window base. An application
normally derives one class from it and stores controls, model state, and signal
handlers in that class.

The application owns the main-window object. `app::run()` borrows it and does
not delete it. The object must stay alive for the complete call, which is why a
local variable in `program()` is the standard pattern.

## Constructors and native creation

The main-window constructor configures portable state and connects signals. It
does not create native resources. At construction time no backend window or
child-parent handle is guaranteed to exist.

Create child controls from `on_wnd_create`, after the main window and its
bindings have been created:

```cpp
class main_window final : public native::app_wnd
{
public:
    main_window()
        : native::app_wnd("Vision"),
          close_button("Close") {
        on_wnd_create.connect(this, &main_window::handle_create);
        close_button.on_click.connect(
            this,
            &main_window::handle_close);
    }

private:
    native::button close_button;

    bool handle_create() {
        close_button.set_parent(this);
        close_button.create();
        close_button.show();
        return false;
    }

    bool handle_close() {
        destroy();
        return true;
    }
};
```

Controls stored as members remain alive while their signal connections and
native resources are active.

## Standard `app::run()` sequence

The shared startup path must execute in this order:

1. Reject a second active application loop and register the borrowed main
   window.
2. Initialize shared application state and refresh the process-owned screen
   snapshot.
3. Create and show the main window.
4. Enter the selected backend's `app::main_loop()`.
5. When the loop ends, destroy remaining native resources, clear the borrowed
   main-window pointer, and return the backend exit code.

The order is part of the public architecture. In particular, screen detection
must precede main-window creation, and the main-window pointer must not remain
published after the run ends.

`app::main_wnd()` returns a borrowed pointer only while `app::run()` is active.
It returns null before startup and after cleanup. Code receiving this pointer
must not store it beyond the active run or delete it.

## Backend main loops

The backend implements `app::main_loop()` because event acquisition is native:
Windows dispatches messages, X11 reads X events, SDL pumps SDL events, and
other toolkits have equivalent mechanisms.

Despite those differences, every backend loop must:

- Dispatch events for the main window and its controls.
- Translate native values to public Native event types.
- Emit public signals synchronously on the UI thread.
- Notice the backend's normal quit condition.
- Return an exit code to the shared `app::run()` cleanup path.

Backends implement only the launcher, event loop, and necessary native hooks.
They must not reorder or bypass shared startup and cleanup.

## Screen availability during startup

The main-window constructor runs before `app::run()`, so it must not assume the
screen snapshot has already been refreshed. The snapshot is available by the
time `on_wnd_create` is emitted. Code that needs the primary work area for
control placement or other initialization can safely query it there.

The complete screen contract is described in
[Screens And Virtual Desktops](patterns-screens.md).
