//
// Declares AppKit text/image cells shared by native tables and outlines.
// Only layout is adapted; AppKit paints the image, text and selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#import <AppKit/AppKit.h>
#include <native.h>

@interface native_content_cell : NSTableCellView
@end

namespace mac
{
    // Return an autoreleased AppKit image from portable straight RGBA.
    NSImage *cell_image(const native::img *source);

    // Populate reusable AppKit subviews, clearing any previous image.
    void configure_cell(NSTableCellView *cell, NSString *text,
                        NSImage *image, NSTextAlignment alignment,
                        bool enabled = true);
}
