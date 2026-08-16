//
// Declares internal macOS native-window bridge types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#import <AppKit/AppKit.h>

#include <string>

namespace native
{
    class app_wnd;
}

namespace mac
{

    class native_window
    {
    public:
        // Create and register an AppKit window for a borrowed owner.
        native_window(native::app_wnd *owner,
                      const char *title,
                      int x,
                      int y,
                      int width,
                      int height);
    };

} // namespace mac
