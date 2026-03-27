#include <string>
#include <native.h>

class menu_window : public native::app_wnd
{
public:
    menu_window()
        : native::app_wnd("Menu Example", 100, 100, 640, 480)
    {
        // Build the menu before the window is created.
        menu << "File"
             << (native::menu_items("New")
                     << std::make_pair(1, std::string("Open..."))
                     << std::make_pair(2, std::string("Save"))
                     << std::make_pair(99, std::string("Exit")))
             << "Edit"
             << (native::menu_items("Cut")
                     << std::string("Copy")
                     << std::string("Paste"))
             << "Help"
             << (native::menu_items("About...")
                     << std::make_pair(100, std::string("License")));

        on_menu.connect(this, &menu_window::on_menu_item);
        on_wnd_paint.connect(this, &menu_window::on_paint);
    }

private:
    int _last_id = 0;

    bool on_menu_item(int id)
    {
        if (id == 99) { destroy(); return true; }
        _last_id = id;
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event e)
    {
        e.g.set_ink(native::rgba(0, 0, 0, 255));
        std::string msg = _last_id > 0
            ? "Selected menu item ID: " + std::to_string(_last_id)
            : "Click a menu item above.";
        e.g.draw_text(msg, native::point(20, 60));
        return true;
    }
};

int program(int argc, char **argv)
{
    (void)argc; (void)argv;
    return native::app::run(menu_window());
}
