#pragma once
#import <AppKit/AppKit.h>
#include <string>

namespace native
{
    class app_wnd;
}

namespace mac
{

    class NativeWindow
    {
    public:
        NativeWindow(native::app_wnd *owner, const char *title, int x, int y, int w, int h);
    };

} // namespace mac
