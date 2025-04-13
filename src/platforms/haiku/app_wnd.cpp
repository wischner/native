#include <native.h>
#include <bindings.h>
#include <Window.h>
#include <iostream>

#include "NativeWindow.h"
#include "globals.h"

namespace native
{

    void app_wnd::create() const
    {
        BRect frame(x(), y(), x() + width() - 1, y() + height() - 1);
        new haiku::NativeWindow(const_cast<app_wnd *>(this), frame, title().c_str());
    }

    void app_wnd::show() const
    {
        BWindow *win = haiku::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!win)
        {
            std::cerr << "Haiku: Can't show window, not created." << std::endl;
            return;
        }

        win->Show();
    }

} // namespace native