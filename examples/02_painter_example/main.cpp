#include "native.h"

class main_wnd : public native::app_wnd
{
public:
    main_wnd() : app_wnd("Custom App Window")
    {
        // connect events, create children, etc.
    }
};

int program(int argc, char *argv[])
{
    return native::app::run(main_wnd());
}