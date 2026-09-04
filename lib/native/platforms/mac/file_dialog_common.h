//
// Declares private AppKit file-panel configuration and completion
// helpers shared by the open and save dialog adapters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#import <AppKit/AppKit.h>

#include <filesystem>
#include <string>

#include <native/file_dialog.h>

namespace mac
{
    // Apply title, initial directory, and portable filters to a panel.
    void configure_file_panel(
        NSSavePanel *panel, const native::file_dialog &dialog);

    // Return a selected URL as a UTF-8 filesystem path.
    std::filesystem::path path_from_url(NSURL *url);

    // Append an extension when a selected filename has no suffix.
    std::filesystem::path add_default_extension(
        const std::filesystem::path &path,
        const std::string &extension);
} // namespace mac
