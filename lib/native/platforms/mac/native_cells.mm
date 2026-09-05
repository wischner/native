//
// Implements reusable AppKit table/outline cells without owner painting.
// Native text fields and image views own text, image and focus rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "native_cells.h"
#include <algorithm>

@implementation native_content_cell
- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setClipsToBounds:YES];
        NSTextField *label = [[NSTextField alloc] initWithFrame:NSZeroRect];
        [label setBezeled:NO];
        [label setEditable:NO];
        [label setSelectable:NO];
        [label setDrawsBackground:NO];
        [label setFont:[NSFont systemFontOfSize:0]];
        [[label cell] setLineBreakMode:NSLineBreakByTruncatingTail];
        [[label cell] setUsesSingleLineMode:YES];
        [self addSubview:label];
        [self setTextField:label];
        [label release];
        NSImageView *image = [[NSImageView alloc] initWithFrame:NSZeroRect];
        [image setImageScaling:NSImageScaleProportionallyDown];
        [self addSubview:image];
        [self setImageView:image];
        [image release];
    }
    return self;
}

- (void)layout {
    [super layout];
    const NSRect bounds = [self bounds];
    const CGFloat icon = std::min<CGFloat>(16, bounds.size.height);
    const bool has_image = [[self imageView] image] != nil;
    [[self imageView] setHidden:!has_image || [[self textField] isHidden]];
    [[self imageView] setFrame:NSMakeRect(2,
        (bounds.size.height - icon) / 2, icon, icon)];
    const CGFloat leading = has_image ? icon + 6 : 2;
    const CGFloat height = std::min<CGFloat>(bounds.size.height,
        [[[self textField] cell] cellSize].height);
    [[self textField] setFrame:NSMakeRect(leading,
        (bounds.size.height - height) / 2,
        std::max<CGFloat>(0, bounds.size.width - leading - 2), height)];
}
@end

namespace mac
{
    NSImage *cell_image(const native::img *source) {
        if (!source || !source->w() || !source->h())
            return nil;
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nullptr
            pixelsWide:source->w() pixelsHigh:source->h()
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSCalibratedRGBColorSpace
            bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
            bytesPerRow:source->w() * 4 bitsPerPixel:32];
        if (!rep)
            return nil;
        auto *bytes = [rep bitmapData];
        for (int y = 0; y < source->h(); ++y) {
            for (int x = 0; x < source->w(); ++x) {
                const auto color = source->pixels()[y * source->w() + x];
                const auto offset = (y * source->w() + x) * 4;
                bytes[offset] = color.r;
                bytes[offset + 1] = color.g;
                bytes[offset + 2] = color.b;
                bytes[offset + 3] = color.a;
            }
        }
        NSImage *image = [[NSImage alloc]
            initWithSize:NSMakeSize(source->w(), source->h())];
        [image addRepresentation:rep];
        [rep release];
        return [image autorelease];
    }

    void configure_cell(NSTableCellView *cell, NSString *text,
                        NSImage *image, NSTextAlignment alignment,
                        bool enabled) {
        [[cell textField] setStringValue:text ? text : @""];
        [[cell textField] setAlignment:alignment];
        [[cell textField] setTextColor:enabled ? [NSColor controlTextColor]
            : [NSColor disabledControlTextColor]];
        [[cell imageView] setImage:image];
        [cell setNeedsLayout:YES];
    }
}
