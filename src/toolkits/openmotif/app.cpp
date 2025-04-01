#include <native.h>
#include <Xm/Xm.h>
#include <iostream>

extern XtAppContext app_context;

namespace native
{
    int app::main_loop()
    {
        XtAppMainLoop(app_context);
        return 0; // never reached unless XtAppSetExitFlag is used
    }
}
