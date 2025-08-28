#include <native.h>
#include <bindings.h>
#include <AppKit/AppKit.h>
#include <iostream>
#include "globals.h"
#include "NativeWindow.h" // ✅ required for the full class definition

namespace native
{

void app_wnd::create() const
{
    new mac::NativeWindow(const_cast<app_wnd *>(this), title().c_str(), x(), y(), width(), height());
}

void app_wnd::show() const
{
    NSWindow *win = mac::wnd_bindings.from_b(const_cast<app_wnd *>(this));
    if (!win)
    {
        std::cerr << "macOS: Can't show window, not created.\n";
        return;
    }

    [win makeKeyAndOrderFront:nil];
}

} // namespace native
