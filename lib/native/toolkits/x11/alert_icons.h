//
// Declares X11-owned monochrome alert bitmaps, copied from GEM assets.
// No GEM code, headers or runtime resources are needed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <X11/Xlib.h>
#include <native/message_box.h>

namespace linux::x11
{
    // Allocate the requested 32-pixel bitmap; caller frees the pixmap.
    Pixmap create_message_icon(Display *display, Drawable drawable,
        native::message_box_icon icon);
}

