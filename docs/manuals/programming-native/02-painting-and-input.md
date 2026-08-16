# Chapter 2: Painting and Mouse Input

Applications usually derive a window class from `native::app_wnd`. The class
stores application state and connects member functions to event signals in
its constructor.

## Signals and handlers

The painter uses four signals:

- `on_mouse_click` starts and ends a stroke.
- `on_mouse_move` appends points while drawing.
- `on_mouse_wheel` clears the drawing.
- `on_wnd_paint` redraws the stored strokes.

A handler returns `true` when it has handled the event and wants signal
propagation to stop. Signal connections do not transfer ownership of the
target object, so the connected window must remain alive.

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
        on_mouse_click.connect(this, &painter_window::on_click);
        on_mouse_move.connect(this, &painter_window::on_move);
        on_mouse_wheel.connect(this, &painter_window::on_wheel);
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

## Invalidation and painting

`invalidate()` schedules the client area for repainting. It does not call the
paint handler synchronously. The backend later emits a `wnd_paint_event`
containing the invalid rectangle and a borrowed graphics context.

The graphics context supports colors, lines, rectangles, text, images, fonts,
and clipping. Never retain the event's graphics reference beyond the handler.
Chapter 10 expands these operations with memory images, PNG/JPEG codecs, font
creation and measurement, and native-look theme primitives.

Next: [Menus and commands](03-menus-and-commands.md).
