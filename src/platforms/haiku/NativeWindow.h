#pragma once

#include <Window.h>
#include <Rect.h>
#include <String.h>

namespace native
{
    class app_wnd;
}

namespace haiku
{

    class NativeWindow : public BWindow
    {
    public:
        NativeWindow(native::app_wnd *owner, BRect frame, const char *title);
        virtual bool QuitRequested() override;
    };

} // namespace haiku
