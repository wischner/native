//
// Implements shared AppKit file-panel configuration, path conversion,
// and cancellation of an active asynchronous sheet.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_common.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <filesystem>
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
                if (extension == "*" || extension == "*.*")
                    return @[];
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
            std::filesystem::path initial = dialog.get_initial_path();
            std::error_code error;
            if (!std::filesystem::is_directory(initial, error))
                initial = initial.parent_path();
            NSString *path = string_from_utf8(initial.string());
            NSURL *url = [NSURL fileURLWithPath:path
                                   isDirectory:YES];
            if (url)
                [panel setDirectoryURL:url];
        }

        NSArray<UTType *> *types = allowed_content_types(dialog);
        if ([types count] != 0)
            [panel setAllowedContentTypes:types];
    }

    std::filesystem::path path_from_url(NSURL *url) {
        if (!url)
            return {};
        const char *path = [[url path] fileSystemRepresentation];
        return path ? std::filesystem::path(path)
                    : std::filesystem::path();
    }

    std::filesystem::path add_default_extension(
        const std::filesystem::path &path,
        const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        std::filesystem::path result(path);
        if (result.has_extension())
            return path;
        result += extension.front() == '.' ? extension
                                           : "." + extension;
        return result;
    }
} // namespace mac

namespace native
{
    void file_dialog::cancel_native_dialog() {
        auto *self = this;
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
