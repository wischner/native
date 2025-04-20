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
    std::vector<native::point> _points;
    bool _drawing;

    // Mouse button handler (start/stop drawing)
    bool on_click(native::mouse_event e)
    {
        if (e.button == native::mouse_button::left)
        {
            _drawing = !_drawing;
            _points.push_back(e.position);
            invalidate({e.position, {1, 1}});
        }
        return true;
    }

    // Track mouse motion and draw when drawing
    bool on_move(native::point p)
    {
        if (_drawing)
        {
            _points.push_back(p);
            invalidate({p, {1, 1}});
        }
        return true;
    }

    // Clear drawing on mouse wheel
    bool on_wheel(native::mouse_wheel_event)
    {
        _points.clear();
        invalidate();
        return true;
    }

    // Paint only visible points
    bool on_paint(native::wnd_paint_event e)
    {
        for (const auto &pt : _points)
        {
            if (e.r.contains(pt))
                e.g.draw_rect({pt, {1, 1}}, true);
        }
        return true;
    }
};

int program(int argc, char *argv[])
{
    return native::app::run(painter_window());
}
