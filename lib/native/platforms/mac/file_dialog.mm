//
// Implements shared AppKit file-panel configuration, path conversion,
// and cancellation of an active asynchronous sheet.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_common.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>

#include <native/file_dialog.h>

#include "globals.h"

namespace
{
    // Convert portable UTF-8 text into an autoreleased NSString.
    NSString *string_from_utf8(const std::string &value) {
        return [NSString stringWithUTF8String:value.c_str()];
    }

    // Convert portable extension filters into AppKit content types.
    NSArray<UTType *> *allowed_content_types(
        const native::file_dialog &dialog) {
        NSMutableArray<UTType *> *types =
            [NSMutableArray array];
        for (const native::file_filter &filter :
             dialog.get_filters()) {
            for (const std::string &pattern : filter.patterns) {
                std::string extension = pattern;
                if (extension.rfind("*.", 0) == 0)
                    extension.erase(0, 2);
                else if (!extension.empty() &&
                         extension.front() == '.')
                    extension.erase(0, 1);
                else
                    continue;

                if (extension.empty() || extension == "*")
                    continue;
                NSString *value = string_from_utf8(extension);
                UTType *type = value
                                   ? [UTType
                                         typeWithFilenameExtension:
                                             value]
                                   : nil;
                if (type && ![types containsObject:type])
                    [types addObject:type];
            }
        }
        return types;
    }
} // namespace

namespace mac
{
    void configure_file_panel(
        NSSavePanel *panel, const native::file_dialog &dialog) {
        if (!panel)
            return;

        NSString *title = string_from_utf8(dialog.get_title());
        if (title)
            [panel setTitle:title];
        [panel setCanCreateDirectories:YES];

        if (!dialog.get_initial_path().empty()) {
            NSString *path =
                string_from_utf8(dialog.get_initial_path());
            BOOL is_directory = NO;
            const BOOL exists = [[NSFileManager defaultManager]
                fileExistsAtPath:path
                     isDirectory:&is_directory];
            if (exists && !is_directory)
                path = [path stringByDeletingLastPathComponent];
            NSURL *url = [NSURL fileURLWithPath:path
                                   isDirectory:YES];
            if (url)
                [panel setDirectoryURL:url];
        }

        NSArray<UTType *> *types = allowed_content_types(dialog);
        if ([types count] != 0)
            [panel setAllowedContentTypes:types];
    }

    std::string path_from_url(NSURL *url) {
        if (!url)
            return {};
        const char *path = [[url path] fileSystemRepresentation];
        return path ? std::string(path) : std::string();
    }

    std::string add_default_extension(
        const std::string &path, const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;

        const std::size_t slash = path.find_last_of("/\\");
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos &&
            (slash == std::string::npos || dot > slash + 1))
            return path;

        std::string result = path;
        if (extension.front() != '.')
            result.push_back('.');
        result += extension;
        return result;
    }
} // namespace mac

namespace native
{
    void file_dialog::cancel_native_dialog() const {
        auto *self = const_cast<file_dialog *>(this);
        NSSavePanel *panel =
            mac::file_dialog_bindings.object_from_handle(self);
        if (!panel)
            return;

        mac::file_dialog_bindings.unregister_by_handle(self);
        [panel cancel:nil];
        [panel orderOut:nil];
        [panel release];
    }
} // namespace native
