#include <native.h>
#include <vector>

class painter_window : public native::app_wnd
{
public:
    painter_window()
        : native::app_wnd("Native Painter"), _drawing(false)
    {
        on_mouse_click.connect(this, &painter_window::on_click);
        on_mouse_move.connect(this, &painter_window::on_move);
        on_mouse_wheel.connect(this, &painter_window::on_wheel);
        on_wnd_paint.connect(this, &painter_window::on_paint);
    }

private:
    std::vector<std::vector<native::point>> _strokes;
    bool _drawing;

    // Left mouse down starts a stroke; left mouse up ends it.
    bool on_click(native::mouse_event e)
    {
        if (e.button == native::mouse_button::left)
        {
            if (e.action == native::mouse_action::press)
            {
                _strokes.push_back({e.position});
                _drawing = true;
            }
            else if (e.action == native::mouse_action::release)
            {
                _drawing = false;
            }
            invalidate();
        }
        return true;
    }

    // Track mouse motion during a stroke.
    bool on_move(native::point p)
    {
        if (_drawing)
        {
            _strokes.back().push_back(p);
            invalidate();
        }
        return true;
    }

    // Mouse wheel clears all strokes.
    bool on_wheel(native::mouse_wheel_event)
    {
        _strokes.clear();
        _drawing = false;
        invalidate();
        return true;
    }

    // Paint all strokes.
    bool on_paint(native::wnd_paint_event e)
    {
        for (const auto &stroke : _strokes)
        {
            for (size_t i = 1; i < stroke.size(); ++i)
                e.g.draw_line(stroke[i - 1], stroke[i]);
        }
        return true;
    }
};

int program(int argc, char *argv[])
{
    return native::app::run(painter_window());
}
