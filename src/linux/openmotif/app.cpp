#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>
#include <iostream>

namespace motif
{
    XtAppContext app_instance;
}

namespace native
{
    int app::main_loop()
    {
        XtAppMainLoop(motif::app_instance);
        return 0;
    }
}