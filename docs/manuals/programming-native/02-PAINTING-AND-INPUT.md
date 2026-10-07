# Chapter 2: Painting and Mouse Input

Applications usually derive a window class from `native::app_wnd`. The class
stores application state and connects member functions to event signals in
its constructor.

## Signals and handlers

The painter uses five signals:

- `on_mouse_click` starts and ends a stroke.
- `on_mouse_move` appends points while drawing.
- `on_mouse_wheel` clears the drawing.
- `on_mouse_cancel` ends an interrupted stroke without a fabricated release.
- `on_wnd_paint` redraws the stored strokes.

A handler returns `true` when it has handled the event and wants signal
propagation to stop. Signal connections do not transfer ownership of the
target object, so the connected window must remain alive.

When the signal source can outlive the receiver, keep a scoped handle as a
receiver member:

```cpp
native::connection _movement =
    source.on_mouse_move.connect_scoped(
        this,
        &painter_window::on_move);
```

Destroying `_movement` disconnects automatically. It is move-only and may be
stored in a `std::vector<native::connection>` when a receiver owns several
subscriptions.

`on_mouse_move` positions are in client coordinates. Native backends also
retain the matching absolute position; `get_mouse_screen_position()` returns
the screen point from the latest motion notification. This is useful for
interactions that move their own top-level window, where adding client
coordinates to a changing window origin would accumulate error.

## Persistent drawing state

Paint handlers should not assume that previous pixels remain available. The
example stores every stroke as points and redraws all strokes whenever the
window is invalidated.

```cpp
//
// Demonstrates retained painting and pointer-event handling with native.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstddef>
#include <vector>

#include <native.h>

class painter_window : public native::app_wnd
{
public:
    // Construct the painter window and connect its event handlers.
    painter_window()
        : native::app_wnd("Native Painter"), _drawing(false) {
        set_cursor(native::mouse_cursor::crosshair);
        on_mouse_click.connect(this, &painter_window::on_click);
        on_mouse_move.connect(this, &painter_window::on_move);
        on_mouse_wheel.connect(this, &painter_window::on_wheel);
        on_mouse_cancel.connect([this](native::mouse_cancel_reason) {
            _drawing = false;
            return false;
        });
        on_wnd_paint.connect(this, &painter_window::on_paint);
    }

private:
    std::vector<std::vector<native::point>> _strokes;
    bool _drawing;

    // Start or finish a stroke when the left button changes state.
    bool on_click(native::mouse_event event) {
        if (event.button == native::mouse_button::left) {
            if (event.action == native::mouse_action::press) {
                _strokes.push_back({event.position});
                _drawing = true;
            }
            else if (event.action == native::mouse_action::release) {
                _drawing = false;
            }
            invalidate();
        }
        return true;
    }

    // Extend the current stroke while the pointer moves.
    bool on_move(native::point position) {
        if (_drawing) {
            _strokes.back().push_back(position);
            invalidate();
        }
        return true;
    }

    // Clear all stored strokes when the wheel is used.
    bool on_wheel(native::mouse_wheel_event) {
        _strokes.clear();
        _drawing = false;
        invalidate();
        return true;
    }

    // Reconstruct the complete drawing during every paint event.
    bool on_paint(native::wnd_paint_event event) {
        for (const auto &stroke : _strokes) {
            for (std::size_t index = 1;
                 index < stroke.size();
                 ++index) {
                event.g.draw_line(
                    stroke[index - 1],
                    stroke[index]);
            }
        }
        return true;
    }
};

// Create the painter window and enter the native event loop.
int program(int, char **) {
    painter_window window;
    return native::app::run(window);
}
```

## Mouse cursors

Every window and control supports the ordinary pointer, text, drawing, and
four directional resize pointer shapes:

```cpp
window.set_cursor(native::mouse_cursor::arrow);
editor.set_cursor(native::mouse_cursor::ibeam);
drawing_surface.set_cursor(native::mouse_cursor::crosshair);
horizontal_splitter.set_cursor(
    native::mouse_cursor::resize_horizontal);
vertical_splitter.set_cursor(native::mouse_cursor::resize_vertical);
northwest_corner.set_cursor(
    native::mouse_cursor::resize_northwest_southeast);
northeast_corner.set_cursor(
    native::mouse_cursor::resize_northeast_southwest);
```

`set_cursor()` may be called before or after `create()` and returns the window
for chaining. `get_cursor()` returns the cached selection. The shape you choose
covers the part of the control you own: on a `canvas` it applies to the client
viewport, while the scrollbars and rulers Native paints keep the ordinary
arrow. Windows default to
the arrow; `text_edit` and `code_edit` choose the I-beam by default. The
painter example selects a crosshair because its client area is a drawing
surface. GEMix falls back to a thin crosshair for resize cursors because AES
does not provide directional system shapes; macOS does the same for diagonal
resize cursors.

## Invalidation and painting

`invalidate()` schedules the client area for repainting. It does not call the
paint handler synchronously. The backend later emits a `wnd_paint_event`
containing the invalid rectangle and a borrowed graphics context.

Backends report keyboard focus through `on_native_focus(bool)` on the same
window contract. Plain windows ignore it; painted controls cache it for their
themed focus stage. Applications normally consume the control's semantic
signals rather than calling this backend-facing hook.

The graphics context supports colors, lines, rectangles, text, images, fonts,
and clipping. Never retain the event's graphics reference beyond the handler.
Chapter 10 expands these operations with memory images, PNG/JPEG codecs, font
creation and measurement, and native-look theme primitives.

Next: [Menus and commands](03-MENUS-AND-COMMANDS.md).

## Physical keyboard input

Connect `on_key` on the focused drawing window to receive `key_event`:
`key` identifies a US reference position, `action` is press/release,
`repeat` identifies a held repeat, and `modifiers` is a typed post-transition
mask. `key_name()` gives a diagnostic name, not text for insertion. Keep
composed text in existing text controls. A handler returning true consumes
the event; input is not automatically forwarded to a parent.

Connect `on_key_reset` to clear held state when focus/input ownership or
lifecycle cancels a session. The library clears its own pairing first and
emits no fabricated releases. Derived focus overrides call their base.
`app::get_physical_keyboard_supported()` is false for current GEMix builds;
AES alone cannot provide physical held-key input. Run Vision with
`--infinity-test` for sided modifiers, repeats, modal cancellation and editor
isolation. See [Background work and services](19-BACKGROUND-WORK-AND-SERVICES.md)
for the accompanying sound and worker diagnostics.

## Hidden client cursors and scoped capture

`mouse_cursor::hidden` is a per-window client policy. It does not hide over
menus, frames, sibling controls or canvas scrollbars. A live policy change
refreshes resolution even if the pointer has not moved. `get_cursor_at()` can
select hidden only inside an application-defined input rectangle, such as an
image viewport. The base answers arrow over chrome/outside points.

Connect `on_mouse_enter` and `on_mouse_leave` to initialize/discard a movement
baseline. `get_mouse_inside()` describes actual hover, independent of capture.
Leave positions may be the last known point. Moving between logical children
emits the old leave before the new enter. Connect `on_mouse_cancel` to clear
held buttons or end a stroke: focus/capture loss and menu/modal takeover do
not fabricate release events. Resource destruction silently invalidates
state without calling back into the dying object.

`on_mouse_motion` offers `dx`/`dy` as floats, explicit absolute/relative kind,
and logical-pixel/device-count units. Relative motion has no valid position
and is never delivered through compatibility `on_mouse_move`. Subscribe to
one motion API so an absolute event does not count twice. Absolute deltas
still derive from Native's existing integer logical coordinates. Entry,
cancellation and capture-mode changes start a zero baseline.

```cpp
std::optional<native::mouse_capture> capture;
std::string error;
auto available = display.get_mouse_capabilities();
if (available.relative_motion) {
    capture = display.capture_mouse(
        {native::mouse_capture_mode::relative, false}, error);
}
// An explicit Escape/menu action releases normally.
capture.reset();
```

Acquire on the UI thread after the display is created, visible and focused.
Only one public lease may be active; an unsupported/competing acquisition
returns no token with a diagnostic and leaves existing pointer state alone.
`get_active()` becomes false after automatic loss. Drag capture continues
outside delivery without confinement; relative mode also hides and confines
as required by its adapter. Provide an escape action before enabling it.
An old token can safely outlive destruction/recreation and never releases a
new lease. Never automatically reacquire after loss of focus.

Current SDL2 supports drag and relative capture; X11-family and Windows support
explicit drag capture. Haiku, macOS and GEMix report explicit capture unavailable.
All adapters report unaccelerated motion unavailable; a request requiring it
fails rather than inventing raw input. Existing private canvas drags still
continue outside the client. Check capabilities instead of testing toolkit names.

`vision --mouse-shaders` exercises these policies beside a native editor and
[programmable image view](20-IMAGE-SHADERS.md). Actual outside/non-client cursor
behavior and confinement require a target desktop test, not a dummy display.
