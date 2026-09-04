//
// Implements AppKit file icons and Foundation special-directory discovery.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "../../filesystem_backend.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    using directory_kind = native::special_directory_kind;

    NSString *native_string(const std::filesystem::path &path) {
        const std::string value = path.string();
        NSString *result = [NSString stringWithUTF8String:value.c_str()];
        return result ? result : @"";
    }

    std::filesystem::path portable_path(NSString *value) {
        return value && [value UTF8String]
                   ? std::filesystem::path([value UTF8String])
                   : std::filesystem::path();
    }

    std::filesystem::path search_path(
        NSSearchPathDirectory directory,
        NSSearchPathDomainMask domain = NSUserDomainMask) {
        NSArray<NSString *> *paths =
            NSSearchPathForDirectoriesInDomains(directory, domain, YES);
        return [paths count] == 0 ? std::filesystem::path()
                                  : portable_path([paths objectAtIndex:0]);
    }

    void append(
        std::vector<native::detail::special_directory_path> &result,
        directory_kind kind,
        std::filesystem::path path) {
        if (!path.empty())
            result.push_back({kind, std::move(path)});
    }

    bool render_icon(NSImage *icon, native::img &target) {
        NSBitmapImageRep *representation =
            [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr
                              pixelsWide:target.w()
                              pixelsHigh:target.h()
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSDeviceRGBColorSpace
                             bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                              bytesPerRow:target.w() * 4
                             bitsPerPixel:32];
        if (!representation)
            return false;

        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext *context = [NSGraphicsContext
            graphicsContextWithBitmapImageRep:representation];
        [NSGraphicsContext setCurrentContext:context];
        [context setImageInterpolation:NSImageInterpolationHigh];
        [[NSColor clearColor] set];
        NSRectFill(NSMakeRect(0, 0, target.w(), target.h()));
        [icon drawInRect:NSMakeRect(0, 0, target.w(), target.h())
                fromRect:NSZeroRect
               operation:NSCompositingOperationSourceOver
                fraction:1.0
          respectFlipped:NO
                   hints:nil];
        [context flushGraphics];
        [NSGraphicsContext restoreGraphicsState];

        const std::uint8_t *source = [representation bitmapData];
        const bool valid = source != nullptr;
        if (source) {
            for (int y = 0; y < target.h(); ++y) {
                for (int x = 0; x < target.w(); ++x) {
                    const std::size_t index =
                        static_cast<std::size_t>(y) * target.w() + x;
                    target.pixels()[index] = native::rgba(
                        source[index * 4],
                        source[index * 4 + 1],
                        source[index * 4 + 2],
                        source[index * 4 + 3]);
                }
            }
        }
        [representation release];
        return valid;
    }
} // namespace

namespace native::detail
{
    bool load_native_file_icon(const std::filesystem::path &path,
                               bool,
                               img &target) {
        @autoreleasepool {
            if (path.empty())
                return false;
            NSImage *icon = [[NSWorkspace sharedWorkspace]
                iconForFile:native_string(path)];
            return icon && render_icon(icon, target);
        }
    }

    std::vector<special_directory_path>
    detect_platform_special_directories() {
        @autoreleasepool {
            std::vector<special_directory_path> result;
            const std::filesystem::path home =
                portable_path(NSHomeDirectory());
            append(result, directory_kind::home, home);
            append(result,
                   directory_kind::desktop,
                   search_path(NSDesktopDirectory));
            append(result,
                   directory_kind::documents,
                   search_path(NSDocumentDirectory));
            append(result,
                   directory_kind::downloads,
                   search_path(NSDownloadsDirectory));
            append(result,
                   directory_kind::music,
                   search_path(NSMusicDirectory));
            append(result,
                   directory_kind::pictures,
                   search_path(NSPicturesDirectory));
            append(result,
                   directory_kind::videos,
                   search_path(NSMoviesDirectory));
            append(result,
                   directory_kind::public_share,
                   home / "Public");
            append(result,
                   directory_kind::applications,
                   search_path(NSApplicationDirectory, NSAllDomainsMask));
            append(result,
                   directory_kind::fonts,
                   search_path(NSLibraryDirectory) / "Fonts");
            append(result,
                   directory_kind::configuration,
                   search_path(NSLibraryDirectory) / "Preferences");
            append(result,
                   directory_kind::application_data,
                   search_path(NSApplicationSupportDirectory));
            append(result,
                   directory_kind::cache,
                   search_path(NSCachesDirectory));
            return result;
        }
    }
} // namespace native::detail
