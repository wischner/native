//
// Implements AppKit file icons and Foundation special-directory discovery.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "../../filesystem_backend.h"

#include <algorithm>
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

    // Resolve lazy workspace representations before rasterizing into a
    // Quartz-supported premultiplied buffer, then publish straight RGBA.
    bool render_icon(NSImage *icon, native::img &target) {
        NSRect proposed = NSMakeRect(0, 0, target.w(), target.h());
        CGImageRef image = [icon CGImageForProposedRect:&proposed context:nil hints:nil];
        if (!image) return false;
        std::vector<std::uint8_t> pixels(std::size_t(target.w()) * target.h() * 4);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(pixels.data(), target.w(), target.h(),
            8, target.w() * 4, space, CGBitmapInfo(kCGImageAlphaPremultipliedLast) | CGBitmapInfo(kCGBitmapByteOrder32Big));
        CGColorSpaceRelease(space);
        if (!context) return false;
        CGContextTranslateCTM(context, 0, target.h());
        CGContextScaleCTM(context, 1, -1);
        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
        CGContextDrawImage(context, CGRectMake(0, 0, target.w(), target.h()), image);
        CGContextRelease(context);
        bool visible = false;
        for (std::size_t index = 0; index < pixels.size() / 4; ++index) {
            const unsigned alpha = pixels[index * 4 + 3];
            auto channel = [&](unsigned offset) {
                return alpha ? std::uint8_t(std::min(255U,
                    (unsigned(pixels[index * 4 + offset]) * 255U + alpha / 2U) / alpha)) : std::uint8_t(0);
            };
            target.pixels()[index] = native::rgba(channel(0), channel(1), channel(2), alpha);
            visible = visible || alpha != 0;
        }
        return visible;
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
                iconForFile:native_string(std::filesystem::absolute(path))];
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
