//
// Implements AppKit general-pasteboard snapshots and publication for
// portable UTF-8 text and lossless PNG image data.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <stdexcept>

#import <AppKit/AppKit.h>

namespace
{
    // Convert an AppKit string to copied UTF-8.
    std::string utf8(NSString *value) {
        if (!value)
            return {};
        const char *text = [value UTF8String];
        return text ? std::string(text) : std::string();
    }

    // Convert TIFF or another AppKit bitmap representation to PNG.
    NSData *png_from_data(NSData *data) {
        if (!data)
            return nil;
        NSBitmapImageRep *bitmap =
            [NSBitmapImageRep imageRepWithData:data];
        return bitmap
                   ? [bitmap representationUsingType:NSBitmapImageFileTypePNG
                                          properties:@{}]
                   : nil;
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        clipboard_payload payload;
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

        NSString *text =
            [pasteboard stringForType:NSPasteboardTypeString];
        if (text) {
            payload.text = utf8(text);
            payload.has_text = true;
        }

        NSData *image =
            [pasteboard dataForType:NSPasteboardTypePNG];
        if (!image) {
            image = png_from_data(
                [pasteboard dataForType:NSPasteboardTypeTIFF]);
        }
        if (image && [image length] != 0) {
            const auto *first = static_cast<const std::uint8_t *>(
                [image bytes]);
            payload.image.assign(first, first + [image length]);
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        NSPasteboardItem *item =
            [[[NSPasteboardItem alloc] init] autorelease];
        if (payload.has_text) {
            NSString *text = [NSString
                stringWithUTF8String:payload.text.c_str()];
            if (!text ||
                ![item setString:text forType:NSPasteboardTypeString]) {
                throw std::runtime_error(
                    "macOS: Unable to stage clipboard text.");
            }
        }
        if (payload.has_image) {
            NSData *data = [NSData dataWithBytes:payload.image.data()
                                          length:payload.image.size()];
            if (!data ||
                ![item setData:data forType:NSPasteboardTypePNG]) {
                throw std::runtime_error(
                    "macOS: Unable to stage clipboard image.");
            }
        }

        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        if (![pasteboard writeObjects:@[ item ]])
            throw std::runtime_error(
                "macOS: Unable to publish clipboard content.");
    }
} // namespace native::detail
