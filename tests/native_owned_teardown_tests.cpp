//
// Destroys an owned window with child controls from posted work, which
// runs outside any toolkit event dispatch. Toolkits that defer widget
// destruction during dispatch free a whole tree at once here.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>

namespace
{
    using namespace native;

    class tool_window final : public modeless_wnd
    {
    public:
        explicit tool_window(app_wnd &owner)
            : modeless_wnd(owner, "tool", 40, 40, 240, 120) {
            on_wnd_create.connect([this] {
                field_.set_parent(this);
                field_.create();
                field_.show();
                command_.set_parent(this);
                command_.create();
                command_.show();
                return true;
            });
        }

    private:
        text_edit field_{"text", text_edit_mode::single_line, 8, 8, 200, 26};
        button command_{"Run", 8, 44, 100, 28};
    };

    class main_test_window final : public app_wnd
    {
    public:
        main_test_window()
            : app_wnd("teardown", 20, 20, 320, 240), tool_(*this) {
            on_wnd_create.connect([this] {
                tool_.create();
                tool_.show();
                app::post([this] {
                    tool_.destroy();
                    app::post([this] { destroy(); });
                });
                return true;
            });
        }

    private:
        tool_window tool_;
    };
} // namespace

int program(int, char **) {
    main_test_window window;
    return app::run(window);
}
