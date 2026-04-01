#include <string>
#include <memory>

#include <native.h>

class layout_absolute_window : public native::app_wnd
{
public:
    layout_absolute_window()
        : native::app_wnd("Layout: Absolute", 100, 100, 520, 280),
          _left("Left", 20, 20, 120, 32),
          _middle("Middle", 180, 70, 140, 32),
          _right("Right", 360, 140, 120, 32)
    {
        on_wnd_create.connect(this, &layout_absolute_window::on_create);
        on_wnd_paint.connect(this, &layout_absolute_window::on_paint);

        _left.on_click.connect(this, &layout_absolute_window::on_any_click);
        _middle.on_click.connect(this, &layout_absolute_window::on_any_click);
        _right.on_click.connect(this, &layout_absolute_window::on_any_click);
    }

private:
    native::button _left;
    native::button _middle;
    native::button _right;
    int _clicks = 0;

    void create_button(native::button &b)
    {
        b.set_parent(this);
        b.create();
        b.show();
    }

    bool on_create()
    {
        create_button(_left);
        create_button(_middle);
        create_button(_right);

        auto layout = std::make_unique<native::absolute_layout_manager>();
        layout->add(_left);            // classic API
        (*layout) << _middle << _right; // DSL API
        set_layout(std::move(layout));

        return true;
    }

    bool on_any_click()
    {
        ++_clicks;
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));
        e.g.draw_text("Absolute layout manager keeps original coordinates.", native::point(20, 205));
        e.g.draw_text("Classic + DSL registration used on the same layout.", native::point(20, 225));
        e.g.draw_text("Clicks: " + std::to_string(_clicks), native::point(20, 245));
        return true;
    }
};

int program(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    layout_absolute_window wnd;
    return native::app::run(wnd);
}
