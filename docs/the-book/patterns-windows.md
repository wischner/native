# Patterns: windows and app windows

Let’s talk about windows. Not the operating system — just windows. And yes, before you ask...

> The base class for all windows in **native** is called `wnd`.  
> You weren’t expecting that, were you?

---

## The `wnd` class

The `wnd` class is the foundational class for all window types in the **native** UI library. Every top-level or child window is derived from this class — even on platforms like SDL where there’s only one visible window.

It is minimal by design, but extensible by intention. Over time, it will grow to support:

- Layout managers
- Menus
- Accelerators
- Child controls
- Focus and keyboard handling
- Custom drawing
- And more

Here’s what it looks like today:

```cpp
class wnd
{
public:
    wnd(int x = 100, int y = 100, int width = 640, int height = 480);
    virtual ~wnd() = default;

    int x() const;
    int y() const;
    int width() const;
    int height() const;

    virtual void create() const = 0;
    virtual void show() const = 0;

    gpx &get_gpx() const;

    signal<> on_wnd_create;
    signal<point> on_wnd_move;
    signal<size> on_wnd_resize;

    signal<point> on_mouse_move;
    signal<mouse_event> on_mouse_click;
    signal<mouse_wheel_event> on_mouse_wheel;

protected:
    int _x, _y, _width, _height;
};
```

This pure C++ class lives in `src/wnd.cpp`, where its members simply store geometry:

```cpp
wnd::wnd(int x, int y, int width, int height)
    : _x(x), _y(y), _width(width), _height(height) {}

int wnd::x() const { return _x; }
int wnd::y() const { return _y; }
int wnd::width() const { return _width; }
int wnd::height() const { return _height; }
```

---

## The `app_wnd` class

The `app_wnd` is a subclass of `wnd`, and represents the main application window — the one passed into `app::run()`.

```cpp
class app_wnd : public wnd
{
public:
    app_wnd(std::string title,
            int x = 100, int y = 100,
            int width = 640, int height = 480);

    virtual ~app_wnd() = default;

    const std::string &title() const;

    virtual void create() const override;
    virtual void show() const override;

private:
    std::string _title;
};
```

The constructor simply stores its title and passes the geometry to `wnd`.

---

## Platform implementations

Each platform or toolkit defines the behavior of `create()` and `show()` through its own native implementation. Let's look at two examples.

### Example 1: Windows (Win32)

#### `create()`:

- Registers a window class if needed
- Creates a native `HWND` with the appropriate styles
- Registers the object in the bindings table
- Subclasses the window procedure to dispatch events via signals

#### `show()`:

- Shows and updates the window
- Uses `ShowWindow()` and `UpdateWindow()`

All event messages like `WM_MOVE`, `WM_SIZE`, `WM_MOUSEMOVE`, and `WM_MOUSEWHEEL` are routed through a custom `WndProc` that emits the proper signal.

### Example 2: SDL2

Even though SDL has no native window hierarchy, `wnd` is still used as a base class. This ensures a consistent application model.

#### `create()`:

- Initializes the SDL video subsystem (if not already done)
- Creates a single `SDL_Window`
- Registers it with the `bindings` system

#### `show()`:

- Shows the window using `SDL_ShowWindow()`
- Clears it with a white background using a `SDL_Renderer`

SDL-specific rendering logic will live in `gpx_img` and `gpx_wnd`.

### Toolkit bindings

Each toolkit registers its bindings in a toolkit-specific file (e.g. `globals.cpp`). All mappings between native handles and C++ objects (like `HWND` ↔ `wnd*`) will be defined there and only there.

This keeps implementation clean, avoids global state spread, and makes it easier to port to new toolkits like OpenLook or Cocoa.

---

## Summary

- The `wnd` class is the foundation for all windows — even on platforms without window hierarchies.
- It defines geometry, virtual `create()` and `show()`, and a set of signals for UI interaction.
- `app_wnd` is a specialization for the main application window, holding a title and owning the event loop.
- Platform-specific behavior is implemented in `create()` and `show()` per toolkit.
- Event routing is done via signal emission in response to native messages.
- All native-to-C++ mappings will be consolidated into `globals.cpp` per toolkit.

Stay tuned — future chapters will cover layout, controls, and painting.
