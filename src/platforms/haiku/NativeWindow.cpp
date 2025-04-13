#include "NativeWindow.h"
#include <native.h>
#include <bindings.h>
#include <iostream>
#include "globals.h"
#include <Application.h>

namespace haiku
{
    NativeWindow::NativeWindow(native::app_wnd *owner, BRect frame, const char *title)
        : BWindow(frame, title, B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE)
    {
        wnd_bindings.register_pair(this, owner);
        Show();
    }

    bool NativeWindow::QuitRequested()
    {
        // For now just quit, later you can emit signals
        be_app->PostMessage(B_QUIT_REQUESTED);
        return true;
    }

} // namespace haiku
