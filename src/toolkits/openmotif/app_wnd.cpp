#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>
#include <iostream>

namespace motif
{
    extern XtAppContext app_instance;
    extern native::bindings<Widget, native::wnd *> wnd_bindings;
}

namespace native
{
    void app_wnd::create() const
    {
        int argc = 0;
        char *argv[] = {nullptr};

        Widget main_wnd = XtVaAppInitialize(
            &motif::app_instance,
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

        if (!main_wnd)
        {
            std::cerr << "Motif: Failed to create top-level shell." << std::endl;
        }
        else
        {
            motif::wnd_bindings
                .register_pair(main_wnd, const_cast<app_wnd *>(this));
        }
    }

    void app_wnd::show() const
    {
        Widget main_wnd =
            motif::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!main_wnd)
        {
            std::cerr << "Motif: Can't show window, not created." << std::endl;
            return;
        }
        XtRealizeWidget(main_wnd);
    }
}