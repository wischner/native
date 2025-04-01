#include "native.h"
#include <Xm/Xm.h>
#include <iostream>

// Define shared Motif context
XtAppContext app_context = nullptr;
Widget top_level_shell = nullptr;

namespace native
{
    void wnd::create() const
    {
        int argc = 0;
        char *argv[] = {nullptr};

        top_level_shell = XtVaAppInitialize(
            &app_context,
            "MotifApp", // application class
            nullptr, 0,
            &argc,
            argv,
            nullptr, // fallback resources
            XtNx, x(),
            XtNy, y(),
            XtNwidth, width(),
            XtNheight, height(),
            nullptr);

        if (!top_level_shell)
        {
            std::cerr << "Motif: Failed to create top-level shell." << std::endl;
        }
    }

    void wnd::show() const
    {
        if (!top_level_shell)
        {
            std::cerr << "Motif: Can't show window, not created." << std::endl;
            return;
        }

        XtRealizeWidget(top_level_shell);
    }
}
