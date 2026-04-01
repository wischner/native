#include <string>

#include <native.h>

class button_window : public native::app_wnd
{
public:
    button_window()
        : native::app_wnd("Button Example", 100, 100, 420, 240),
          _button("Click me", 20, 20, 120, 32)
    {
        on_wnd_create.connect(this, &button_window::on_create);
        on_wnd_paint.connect(this, &button_window::on_paint);
        _button.on_click.connect(this, &button_window::on_button_click);
    }

private:
    native::button _button;
    int _click_count = 0;

    bool on_create()
    {
        _button.set_parent(this);
        _button.create();
        _button.show();
        return true;
    }

    bool on_button_click()
    {
        ++_click_count;
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));

        std::string message = "Button clicks: " + std::to_string(_click_count);
        e.g.draw_text(message, native::point(20, 80));
        e.g.draw_text("Press the button above.", native::point(20, 110));
        return true;
    }
};

int program(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    button_window wnd;
    return native::app::run(wnd);
}
