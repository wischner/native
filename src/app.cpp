#include "native.h"

namespace native
{
    int app::run(const app_wnd &wnd)
    {
        wnd.create();
        wnd.show();
        return app::main_loop();
    }
}
