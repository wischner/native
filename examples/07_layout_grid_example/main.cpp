#include <string>
#include <memory>

#include <native.h>

class layout_grid_window : public native::app_wnd
{
public:
    layout_grid_window()
        : native::app_wnd("Layout: Grid", 100, 100, 760, 440),
          _toolbar("Toolbar", 0, 0, 120, 32),
          _left_panel("Left Panel", 0, 0, 120, 32),
          _status("Status", 0, 0, 120, 32),
          _g1("G1", 0, 0, 80, 28),
          _g2("G2", 0, 0, 80, 28),
          _g3("G3", 0, 0, 80, 28),
          _g4("G4", 0, 0, 80, 28)
    {
        on_wnd_create.connect(this, &layout_grid_window::on_create);
        on_wnd_paint.connect(this, &layout_grid_window::on_paint);

        _toolbar.on_click.connect(this, &layout_grid_window::on_any_click);
        _left_panel.on_click.connect(this, &layout_grid_window::on_any_click);
        _status.on_click.connect(this, &layout_grid_window::on_any_click);
        _g1.on_click.connect(this, &layout_grid_window::on_any_click);
        _g2.on_click.connect(this, &layout_grid_window::on_any_click);
        _g3.on_click.connect(this, &layout_grid_window::on_any_click);
        _g4.on_click.connect(this, &layout_grid_window::on_any_click);
    }

private:
    native::button _toolbar;
    native::button _left_panel;
    native::button _status;
    native::button _g1;
    native::button _g2;
    native::button _g3;
    native::button _g4;

    int _clicks = 0;

    void create_button(native::button &b)
    {
        b.set_parent(this);
        b.create();
        b.show();
    }

    bool on_create()
    {
        create_button(_toolbar);
        create_button(_left_panel);
        create_button(_status);
        create_button(_g1);
        create_button(_g2);
        create_button(_g3);
        create_button(_g4);

        auto root = std::make_unique<native::grid_layout_manager>();

        // Root grid: fixed toolbar + star content + fixed status.
        (*root)
            << native::row(native::pixels(48))
            << native::row(native::star())
            << native::row(native::pixels(52))
            << native::column(native::star(1.0f))
            << native::column(native::star(2.0f))
            << native::cell(_toolbar, 0, 0, 1, 2, 8)
            << native::cell(_left_panel, 1, 0, 1, 1, 8)
            << native::cell(_status, 2, 0, 1, 2, 8);

        // Child grid inside root cell (1,1): 2x2 star matrix.
        auto child = std::make_unique<native::grid_layout_manager>(2, 2);
        child->add(_g1, 0, 0, 1, 1, 8)
             .add(_g2, 0, 1, 1, 1, 8)
             .add(_g3, 1, 0, 1, 1, 8)
             .add(_g4, 1, 1, 1, 1, 8);

        (*root) << native::child_grid(std::move(child), 1, 1, 1, 1, 4);

        set_layout(std::move(root));
        return true;
    }

    bool on_any_click()
    {
        ++_clicks;
        _status.set_text("Status: clicks=" + std::to_string(_clicks));
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));
        e.g.draw_text("Grid layout: star/pixel tracks + nested child grid.", native::point(16, 74));
        e.g.draw_text("Resize the window to see automatic re-layout.", native::point(16, 94));
        return true;
    }
};

int program(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    layout_grid_window wnd;
    return native::app::run(wnd);
}
