//
// Declares the private BeAPI file-panel launcher shared by portable
// open and save dialog objects.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/file_dialog.h>

namespace haiku
{
    // Create and show one asynchronous BeAPI file panel.
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool allow_multiple,
                          const std::string &suggested_name,
                          const std::string &default_extension,
                          bool directory = false);
} // namespace haiku
