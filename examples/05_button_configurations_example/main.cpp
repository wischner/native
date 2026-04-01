#include <string>

#include <native.h>

class button_configurations_window : public native::app_wnd
{
public:
    button_configurations_window()
        : native::app_wnd("Button Configurations", 100, 100, 560, 320),
          _default_size("Default size", 20, 20),
          _point_size("Point + size", native::point(150, 20), native::size(140, 32)),
          _rect_ctor("Rect ctor", native::rect(310, 20, 120, 32)),
          _rename_button("Rename first", 20, 70, 130, 32),
          _title_button("Update title", 170, 70, 130, 32)
    {
        on_wnd_create.connect(this, &button_configurations_window::on_create);
        on_wnd_paint.connect(this, &button_configurations_window::on_paint);

        _default_size.on_click.connect(this, &button_configurations_window::on_default_click);
        _point_size.on_click.connect(this, &button_configurations_window::on_point_click);
        _rect_ctor.on_click.connect(this, &button_configurations_window::on_rect_click);
        _rename_button.on_click.connect(this, &button_configurations_window::on_rename_click);
        _title_button.on_click.connect(this, &button_configurations_window::on_title_click);
    }

private:
    native::button _default_size;
    native::button _point_size;
    native::button _rect_ctor;
    native::button _rename_button;
    native::button _title_button;

    int _total_clicks = 0;
    int _rename_count = 0;

    void attach_button(native::button &btn)
    {
        btn.set_parent(this);
        btn.create();
        btn.show();
    }

    bool on_create()
    {
        attach_button(_default_size);
        attach_button(_point_size);
        attach_button(_rect_ctor);
        attach_button(_rename_button);
        attach_button(_title_button);
        return true;
    }

    bool on_default_click()
    {
        ++_total_clicks;
        invalidate();
        return true;
    }

    bool on_point_click()
    {
        ++_total_clicks;
        _point_size.set_text("Point + size [clicked]");
        invalidate();
        return true;
    }

    bool on_rect_click()
    {
        ++_total_clicks;
        _rect_ctor.set_text("Rect ctor [clicked]");
        invalidate();
        return true;
    }

    bool on_rename_click()
    {
        ++_total_clicks;
        ++_rename_count;
        _default_size.set_text("Default #" + std::to_string(_rename_count));
        invalidate();
        return true;
    }

    bool on_title_click()
    {
        ++_total_clicks;
        set_title("Button Configurations (" + std::to_string(_total_clicks) + ")");
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));
        e.g.draw_text("Configuration demo:", native::point(20, 130));
        e.g.draw_text("- ctor with default dimensions", native::point(20, 155));
        e.g.draw_text("- ctor with point + size", native::point(20, 178));
        e.g.draw_text("- ctor with rect", native::point(20, 201));
        e.g.draw_text("- runtime reconfiguration via set_text/set_title", native::point(20, 224));
        e.g.draw_text("Total clicks: " + std::to_string(_total_clicks), native::point(20, 260));
        return true;
    }
};

int program(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    button_configurations_window wnd;
    return native::app::run(wnd);
}
