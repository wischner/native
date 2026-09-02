//
// Declares the private Motif FileSelectionBox launcher shared by the
// portable open and save dialog classes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/file_dialog.h>

namespace linux::openmotif
{
    // Create and manage one Motif file-selection dialog.
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory = false);
} // namespace linux::openmotif
